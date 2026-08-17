// Copyright 2026 devMiyax
//
// Vdp2Compositor implementation (issue #22, task T-008). See Vdp2Compositor.h
// for the design summary and docs/feature/issue-22/01-design.md section 2.5.
//
// The fragment shader's basic-pattern selection mirrors vdp2_color_oracle.h
// compositeBasic() 1:1 -- keep the two in sync (host-port UT-005/B01/B02/E03/E04
// in vulkan/test/vdp2_compositor_test.cpp pin the reference). ASCII-only
// comments (CLAUDE.md rule: MSVC CP932 -> C4819/C2065).

#include "Vdp2Compositor.h"
#include "Vdp2GBuffer.h"
#include "VIDVulkan.h"
#include "VulkanTools.h"

#include "shaderc/shaderc.hpp"

#include <array>
#include <cstring>
#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Shader sources. A vertex-buffer-less fullscreen triangle: gl_VertexIndex in
// {0,1,2} produces clip-space coords covering the whole screen, and v_uv the
// matching [0,1] texture coordinate. No vertex/index buffers are needed.
// ---------------------------------------------------------------------------
static const char* kVertSrc = R"S(
// issue #22: the compositor draws this fullscreen triangle straight into the
// window / subRenderTarget pass, so -- exactly like the legacy priority loop's
// pre_rotate_mat (VIDVulkan::Vdp2DrawEnd) -- it must pre-rotate its clip-space
// position to match the Android swapchain pre-transform (surface currentTransform,
// ROTATE_90/270 only in landscape) and rotate_screen (tate). Without this the new
// VDP2 composite path renders 90 deg sideways in landscape. pc.rot = (cos, sin) of
// the net angle; mat2(c, s, -s, c) is column-major glm R(angle) = [[c,-s],[s,c]],
// matching pre_rotate_mat. Identity (cos 1, sin 0) when no rotation is needed.
layout(push_constant) uniform Push {
  vec2 rot;
} pc;
layout(location = 0) out vec2 v_uv;
void main() {
  // Fullscreen triangle (Bilodeau): index 0 -> (-1,-1), 1 -> (3,-1), 2 -> (-1,3)
  v_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  vec2 p = v_uv * 2.0 - 1.0;
  p = mat2(pc.rot.x, pc.rot.y, -pc.rot.y, pc.rot.x) * p;
  gl_Position = vec4(p, 0.0, 1.0);
}
)S";

// Basic + normal color-calc compositor. Mirrors vdp2_color_oracle.h
// compositeBasic() (T-008) and compositeNormal() (T-012) 1:1:
//   for priority 7..1: for which kSprite..kNBG3:
//     skip transparent / priority 0; first match -> top, second match -> second.
//   no opaque layer -> back screen.
// Then, if the top layer's color calc applies (titan transTest), blend top with
// second (ratio / add per CCMD, ratio source per CCRTMD). Otherwise top passes
// through unchanged (basic pattern).
//
// Layer slice indices match vdp2oracle::LayerId: 0 NBG3, 1 NBG2, 2 NBG1,
// 3 NBG0, 4 RBG0, 5 Sprite. kSprite == 5, kNBG3 == 0. Attr bit layout matches
// packAttr(): bits 0-4 priority, bit 5 ccEnable, bits 6-11 ccRatio, bit 12
// transparent. Priority is clamped to [0,7] exactly like clampPriority().
//
// Blend math is the host-port titan reference (blendTop / blendBottom /
// blendAdd in vdp2_color_oracle.h). The per-channel arithmetic uses integer
// 0..255 values to match the host rounding exactly (1 LSB tolerance):
//   ratio path: ca = (ccRatio<<2)+3; out = (top*ca + bottom*(255-ca)) / 255
//   add path  : out = min(top + bottom, 255)
// ccRatio is taken from the top layer (CCRTMD=Top) or the second layer
// (CCRTMD=Second); titan blendBottom additionally requires the top "trans"
// (bit 31 == ccEnable) bit, which the gate below already enforces for that mode.
static const char* kFragSrc = R"S(
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform Ubo {
  vec4 u_backColor;          // rgb in xyz
  int u_colorCalcMode;       // 0 = ratio, 1 = add (CCMD)
  int u_ratioFromSecond;     // 0 = ratio from top, 1 = from second (CCRTMD)
  int u_exccEnable;          // EXCCEN (CCCTL bit 10): extended color calc on
  int u_lineColorInserted;   // LNCL inserted (table 12.2 branch; T-015)
  int u_vheight;             // vdp2height: Saturn line count for LNCL row index
  int u_debugViewSlice;      // >=0: output that G-buffer slice raw (debug)
  int u_lineScreenMask;      // bit[slice]=1 if that layer has LNCL (from LNCLEN)
  int u_lineColorAlpha;      // line color cc ratio ((~CCRLB)&0x1F)<<1, 0..62 (inverted encoding, like layer ratios)
  // CRAM color mode (RAMCTL bits 13:12) and per-slice palette-format mask. Table
  // 12.2 (issue #22): the extended-cc ratio degrades to 4:0:0 in CRAM mode 1/2
  // when the second image is palette format (and inserting a line color forces
  // the second image to palette). u_paletteFormatMask bit[slice]=1 => that
  // layer is palette format (color mode 0/1/3); 0 => direct RGB (2/4).
  int u_cramMode;
  int u_paletteFormatMask;
  int u_hiresCram12;         // MIXIT_SPECIAL_HIRES_CRAM12: suppress palette second
  // Color gradation / blur (BOKEN, CCCTL bit 15, CRAM mode 0). When u_gradEnable
  // is set and the top or second layer is the blur-source slice u_gradSlice, the
  // second operand color is replaced by a horizontal 3-tap blur of that slice;
  // u_vwidth (Saturn dot width) sets the tap spacing. Mutually exclusive with EXCC.
  int u_gradEnable;
  int u_gradSlice;
  int u_vwidth;
  // Per-line back screen (BKCLMD, BKTAU bit 15; VDP2 manual section 7.2).
  // When set, the back screen color varies per scan line (e.g. the Space
  // Harrier horizon gradient) and the shader reads it from ROW 4 of
  // s_lineColor instead of the frame-constant u_backColor.
  int u_backPerLine;
  int u_pad2;
  // Color offset (Sega VDP2 manual ch.13): a signed -255..255 RGB offset added
  // to the TOP image's final color AFTER color calc, gated by the TOP layer's
  // CLOFEN bit (CLOFSL selects A/B). Precomputed per G-buffer slice on the host
  // (0 when that layer's CLOFEN bit is clear). [.w] unused. u_backColorOffset is
  // applied when every layer is transparent (back screen is the top image).
  ivec4 u_layerColorOffset[6];
  ivec4 u_backColorOffset;
  // Color calc window (ch.12, WCTLD>>8): x=winmask, y=winflag, z=winmode
  // (-1 = none). Outside the cc-window valid area color calc is skipped.
  ivec4 u_ccWindow;
  // Mosaic (ch.10, MZCTL): x=global block width, y=global block height,
  // z=per-slice enable mask. Enabled slices snap the sample to the block.
  ivec4 u_mosaic;
} ubo;

// G-buffer slices (kSliceCount = 6) as 2D array textures.
layout(binding = 1) uniform sampler2DArray s_color;
layout(binding = 2) uniform usampler2DArray s_attr;

// Line color screen (LNCL). The line color table is stored in ROW 0 of a
// 512-wide texture, indexed by the Saturn scan-line number on the x axis (same
// convention as VdpBack / the per-line shaders: linepos.y = 0, linepos.x =
// line). The compositor draws an HD-scaled fullscreen triangle, so the Saturn
// line is recovered from v_uv.y * vdp2height (T-015). A single-color line screen
// stores the same value for every line.
layout(binding = 3) uniform sampler2D s_lineColor;

const int kSliceCount = 6;
const int kSprite = 5;
const int kMaxPriority = 7;

uint clampPriority(uint p) {
  return (p > uint(kMaxPriority)) ? uint(kMaxPriority) : p;
}

// issue #22 mosaic: snap a slice's sample UV to the global block top-left when
// that slice has mosaic enabled (u_mosaic.z bit). The block size (u_mosaic.xy)
// is in Saturn dots, so convert v_uv -> Saturn dot -> snap -> back to UV. The
// slices are rendered un-mosaic'd, so this read-time snap replicates the block
// top-left dot across the block (Sega VDP2 manual ch.10). Slices without mosaic
// (and the whole frame when u_mosaic.z == 0) read v_uv unchanged -- no-op.
vec2 uvFor(int which) {
  if (((ubo.u_mosaic.z >> which) & 1) == 0) { return v_uv; }
  float w = float(ubo.u_vwidth);
  float h = float(ubo.u_vheight);
  float mx = float(ubo.u_mosaic.x);
  float my = float(ubo.u_mosaic.y);
  float sx = floor(floor(v_uv.x * w) / mx) * mx;
  float sy = floor(floor(v_uv.y * h) / my) * my;
  return vec2((sx + 0.5) / w, (sy + 0.5) / h);
}

// Attr fetch for a slice, mosaic-aware (uvFor).
uint attrAt(int which) {
  return texture(s_attr, vec3(uvFor(which), float(which))).r;
}

// Fetch the layer color as 0..255 integer rgb (matches the host getRed/Green/
// Blue on the texelFetched RGBA8 value). Mosaic-aware via uvFor.
ivec3 colorRGB(int which) {
  vec3 c = texture(s_color, vec3(uvFor(which), float(which))).rgb;
  return ivec3(clamp(c * 255.0 + 0.5, vec3(0.0), vec3(255.0)));
}

// Same, sampling at an explicit uv (used by the gradation blur to read the
// blur-source slice at the two previous Saturn dots).
ivec3 colorRGBAt(int which, vec2 uv) {
  vec3 c = texture(s_color, vec3(uv, float(which))).rgb;
  return ivec3(clamp(c * 255.0 + 0.5, vec3(0.0), vec3(255.0)));
}

// the SW reference ss/vdp2_render.cpp T_MixIt ratio blend (the authoritative reference;
// see Vdp2ColorCalcState.h). the SW reference: fore_ratio = (rawCCR & 0x1F) ^ 0x1F,
// sec_ratio = 0x20 - fore_ratio, out = (fore*fore_ratio + sec*sec_ratio) >> 5
// (32 levels). The gbuffer ccRatio is (ratio5 << 1) + 1 for layers and
// (lineRatio5 << 1) for the line color, where ratio5 == the SW reference fore_ratio; a
// single `>> 1` recovers fore_ratio for both encodings (layer values are odd, the
// line value is even, so the truncating shift drops the +1 only where present).
// The ratio is supplied by the caller from whichever operand CCRTMD selects.
// top/bottom are 0..255 rgb.
ivec3 blendRatio(ivec3 top, ivec3 bottom, int ccRatio) {
  int fr = ccRatio >> 1;               // the SW reference fore_ratio (0..31)
  int sr = 0x20 - fr;                  // the SW reference sec_ratio
  return (top * fr + bottom * sr) >> 5;
}

// titan blendAdd body: saturating add, ratio ignored.
ivec3 blendAdd(ivec3 top, ivec3 bottom) {
  return min(top + bottom, ivec3(255));
}

// Line color for the current row (LNCL). The line color table lives in row 0 of
// s_lineColor, indexed by the Saturn scan line on x. The fullscreen draw may be
// HD-scaled, so recover the scan line from v_uv.y * u_vheight and clamp to the
// table width. Returns 0..255 rgb. Shared by the normal-path line color screen
// (titan.c:324) and the extended-chain LNCL fold (figure 12.3).
ivec3 lineColorForRow() {
  int lineSize = textureSize(s_lineColor, 0).x;
  int lineIdx = int(v_uv.y * float(ubo.u_vheight));
  if (lineIdx < 0) { lineIdx = 0; }
  if (lineIdx >= lineSize) { lineIdx = lineSize - 1; }
  vec3 lc = texelFetch(s_lineColor, ivec2(lineIdx, 0), 0).rgb;
  return ivec3(clamp(lc * 255.0 + 0.5, vec3(0.0), vec3(255.0)));
}

// Back screen color for the current row (0..255 rgb, RAW -- the BKCOEN color
// offset is applied by the caller via u_backColorOffset). Single-color mode
// (BKCLMD = 0) uses the frame-constant u_backColor; per-line mode (BKCLMD = 1,
// u_backPerLine set) reads the per-line color the host packed into ROW 4 of
// s_lineColor (same scan-line-on-x convention as the line color table in row
// 0). VDP2 manual section 7.2 (Space Harrier horizon gradient).
ivec3 backColorForRow() {
  if (ubo.u_backPerLine == 0) {
    return ivec3(clamp(ubo.u_backColor.rgb * 255.0 + 0.5, vec3(0.0), vec3(255.0)));
  }
  int lineSize = textureSize(s_lineColor, 0).x;
  int lineIdx = int(v_uv.y * float(ubo.u_vheight));
  if (lineIdx < 0) { lineIdx = 0; }
  if (lineIdx >= lineSize) { lineIdx = lineSize - 1; }
  vec3 bc = texelFetch(s_lineColor, ivec2(lineIdx, 4), 0).rgb;
  return ivec3(clamp(bc * 255.0 + 0.5, vec3(0.0), vec3(255.0)));
}

// Extended color calc -- build the "extended second" (the operand the TOP image
// is color-calc'd against). This is a faithful port of the SW reference ss/vdp2_render.cpp
// T_MixIt() (the pix2 construction inside the color-calc block), which is the
// authoritative reference for figure 12.3 behaviour.
//
// The SW reference stacks the displayed images by priority: pix = top, pix2 = priority
// second, pix3 = priority third. When the TOP layer has the line color screen
// inserted (its LNCLEN bit set, lineInsert == true), the LINE COLOR is inserted
// as the new second and the priority-second / priority-third shift DOWN one slot:
//   new second = lineColor, new third = (old) priority-second, new fourth = (old)
//   priority-third.
// The "extended second" the top blends against is then:
//
//   Line color inserted:
//     CRAM mode 0 (EXCC_LINE_CRAM0):
//       ext2 = (lineColor + (secondCc ? second/2 : second)) / 2
//     CRAM mode 1/2 (EXCC_LINE_CRAM12):
//       second RGB     -> ext2 = (lineColor + ((secondCc && thirdRGB)
//                                              ? (second+third)/2 : second)) / 2
//       second palette -> ext2 = lineColor          (priority-second excluded)
//
//   No line color inserted (EXCC_CRAM0 / _CRAM12), Table 12.2:
//     secondCc && (CRAM mode 0 || (secondRGB && thirdRGB)) -> (second + third)/2
//     otherwise                                            -> second (4:0:0)
//   In CRAM mode 1/2 a palette-format second OR third forces 4:0:0; CRAM mode 0
//   ignores the formats (fold iff secondCc). Table 12.2 is the primary source --
//   vidsoft / the SW reference do not bit-reproduce the extended-cc ratio.
//
// The key correction over the previous (manual-paraphrase) implementation: the
// line color folds with the PRIORITY-SECOND image (NBG0), not with idx[2]; with
// no opaque third the second is still kept (CRAM0) rather than dropped. second /
// third / lineColor are 0..255 rgb; a missing layer is black. cram0 == (CRAM mode
// 0); secondRGB / thirdRGB are the per-slice direct-color (non-palette) flags.
ivec3 buildExtendedSecond(ivec3 second, ivec3 third,
                          bool secondCc, bool secondRGB, bool thirdRGB,
                          bool lineInsert, ivec3 lineColor, bool cram0) {
  if (lineInsert) {
    if (cram0) {
      ivec3 t = secondCc ? (second >> 1) : second;
      return min((lineColor + t) / 2, ivec3(255));
    }
    if (secondRGB) {
      ivec3 t = (secondCc && thirdRGB) ? ((second + third) / 2) : second;
      return min((lineColor + t) / 2, ivec3(255));
    }
    return min(lineColor, ivec3(255));            // palette second excluded
  }
  if (secondCc && (cram0 || (secondRGB && thirdRGB))) {
    return min((second + third) / 2, ivec3(255));
  }
  return min(second, ivec3(255));
}

// Final top<->second blend stage (titan transTest gate + ratio/add), shared by
// the normal and extended paths. Mirrors oracle blendTopSecondStage().
ivec3 blendTopSecondStage(ivec3 topColor, int topRatio, bool topCcEnable,
                          ivec3 secondColor, int secondRatio) {
  // the SW reference T_MixIt gates the top<->second blend purely on the TOP layer's
  // color-calc-enable bit (pix & PIX_CCE_SHIFT), independent of CCMD/CCRTMD. In
  // the ratio+Top case this is equivalent to the old `topRatio < 0x3F` gate (a
  // cc-disabled layer decodes to ratio 0x3F), so the unification is behaviour-
  // preserving there and matches the SW reference for the Add / CCRTMD=Second cases.
  if (!topCcEnable) {
    return topColor;                  // basic pattern: top passes through
  }
  if (ubo.u_colorCalcMode == 1) {
    return blendAdd(topColor, secondColor);
  }
  int ratio = (ubo.u_ratioFromSecond != 0) ? secondRatio : topRatio;
  return blendRatio(topColor, secondColor, ratio);
}

// Select up to four opaque layers by priority (descending) with the LayerId
// tiebreak (Sprite > RBG0 > NBG0..3), optionally skipping one slice. idx[k] is
// the slice of the k-th layer, or -1. Used twice for shadow resolution: once
// normally, then again skipping a topmost shadow caster (G4).
void selectLayers(int skipSlice, out int idx[4]) {
  idx = int[4](-1, -1, -1, -1);
  int found = 0;
  for (int priority = kMaxPriority; priority > 0 && found < 4; --priority) {
    for (int which = kSprite; which >= 0 && found < 4; --which) {
      if (which == skipSlice) { continue; }
      uint attr = attrAt(which);
      bool transparent = (attr & (1u << 12)) != 0u;
      if (transparent) { continue; }
      uint prio = clampPriority(attr & 0x1Fu);
      if (prio == 0u) { continue; }
      if (int(prio) != priority) { continue; }
      idx[found] = which;
      found += 1;
    }
  }
}
)S"
// Split here: MSVC caps a single string literal at ~16 KB (C2026). Adjacent
// raw-string literals concatenate at compile time into one GLSL source.
R"S(
void main() {
  // issue #22 debug: raw G-buffer slice viewer. When u_debugViewSlice >= 0,
  // bypass color calc and show that slice's stored color (or solid green where
  // the slice is transparent / never written) so a "drawn but empty" slice is
  // visually distinguishable from a populated one.
  if (ubo.u_debugViewSlice >= 0) {
    int dbgWhich = ubo.u_debugViewSlice;
    uint dbgAttr = texture(s_attr, vec3(v_uv, float(dbgWhich))).r;
    if ((dbgAttr & (1u << 12)) != 0u) {
      outColor = vec4(0.0, 0.5, 0.0, 1.0);          // green = transparent/empty
    } else {
      vec3 dbgCol = texture(s_color, vec3(v_uv, float(dbgWhich))).rgb;
      outColor = vec4(dbgCol, 1.0);                 // raw slice color
    }
    return;
  }

  // Per-line color-calc flags (issue #22). CCCTL is a per-line register, so the
  // global CC gates EXCCEN / CCMD / CCRTMD and the LNCL-into-extended gate vary
  // by scan line. The host packs them per line into row 1 of s_lineColor (R
  // channel byte: bit0 EXCCEN, bit1 LNCL-inserted, bit2 CCMD, bit3 CCRTMD). Use
  // the per-line EXCCEN / LNCL-inserted to drive the extended path below; CCMD /
  // CCRTMD stay on the global ubo (the blend helpers read those) -- they are
  // frame-constant in the cases seen so far.
  int ccRow = int(v_uv.y * float(ubo.u_vheight));
  {
    int lcW = textureSize(s_lineColor, 0).x;
    if (ccRow < 0) { ccRow = 0; }
    if (ccRow >= lcW) { ccRow = lcW - 1; }
  }
  vec4 ccTexel = texelFetch(s_lineColor, ivec2(ccRow, 1), 0);
  uint ccFlags = uint(ccTexel.r * 255.0 + 0.5);
  int exccEnableRow = int(ccFlags & 1u);
  int lineColorInsertedRow = int((ccFlags >> 1) & 1u);
  // Per-line line-color-screen per-layer mask (G channel, vdp2cc slice bits).
  // LNCLEN is per-line, so the lineInsert / normal-path LNCL gates below use this
  // per-row mask, not the frame-global u_lineScreenMask (issue #22, Lunar).
  int lineScreenMaskRow = int(ccTexel.g * 255.0 + 0.5);

  // Select up to four opaque layers with the descending-priority /
  // LayerId-tiebreak loop (extended cc needs top..fourth; normal cc only uses
  // the first two). idx[k] = slice of the k-th layer, or -1.
  int idx[4];
  selectLayers(-1, idx);

  // Shadow (G4): when the topmost opaque layer is a shadow caster (attr bit 13,
  // sprite MSB / shadow pixel), drop it and re-select the layers below; if the
  // new topmost accepts the shadow (attr bit 14, SDCTL) the final composite is
  // darkened by half luminance. Mirrors the reference renderer's PIX_DOSHAD
  // "draw the layer below, then halve it if it has SHADEN" behaviour.
  bool shadowHalfLum = false;
  if (idx[0] >= 0) {
    uint a0 = attrAt(idx[0]);
    if ((a0 & (1u << 13)) != 0u) {
      int shadowSlice = idx[0];
      selectLayers(shadowSlice, idx);
      if (idx[0] >= 0) {
        uint nb = attrAt(idx[0]);
        shadowHalfLum = (nb & (1u << 14)) != 0u;
      }
    }
  }

  // All transparent -> back screen. Color offset (ch.13) applies to the back
  // when it is the displayed top image (BKCOEN -> u_backColorOffset).
  if (idx[0] < 0) {
    ivec3 bc = backColorForRow();
    bc = clamp(bc + ubo.u_backColorOffset.rgb, ivec3(0), ivec3(255));
    outColor = vec4(vec3(bc) / 255.0, 1.0);
    return;
  }

  uint topAttr = attrAt(idx[0]);
  bool topCcEnable = (topAttr & (1u << 5)) != 0u;
  int topRatio = int((topAttr >> 6) & 0x3Fu);
  ivec3 topColor = colorRGB(idx[0]);

  // Color calc window (ch.12, WCTLD>>8). Evaluated BEFORE the LNCL insertion
  // and extended-cc stages: on hardware the cc window clears the pixel's
  // color-calc-enable bit itself (mednafen ApplyWin clears only PIX_CCE), so
  // every color-calc consumer -- the normal-path LNCL line-color blend below,
  // the extended fold, and the final top<->second stage -- is suppressed
  // through the topCcEnable gate. Gating only the final blend (the previous
  // placement) let the LNCL insertion darken the whole screen when a game
  // restricts it to a window (EMIT Vol.1 subtitle band). The W0/W1 horizontal
  // spans live in rows 2/3 of s_lineColor (same packing as the layer display
  // window) and the sprite window comes from the sprite slice's attr bit 15;
  // u_ccWindow.x == 0 means no cc window (no behavior change).
  // u_ccWindow.z: 0 = AND, 1 = OR, 2 = deny everywhere (the hardware case where
  // the logic bit is set with no window enabled -- color calc off screen-wide).
  if (ubo.u_ccWindow.x != 0) {
    bool _ccAllows;
    if (ubo.u_ccWindow.z == 2) {
      _ccAllows = false;
    } else {
      // _cwx is only compared against the span bounds, it never indexes the
      // table, so it must be clamped to the SCREEN width. Clamping it to the
      // line-color texture width (a fixed 512) made every pixel right of x=511
      // inherit the membership computed at x=511 in 704-dot modes.
      int _cwx = int(v_uv.x * float(ubo.u_vwidth));
      if (_cwx < 0) { _cwx = 0; }
      if (_cwx > ubo.u_vwidth - 1) { _cwx = ubo.u_vwidth - 1; }
      uvec4 _cw0 = uvec4(texelFetch(s_lineColor, ivec2(ccRow, 2), 0) * 255.0 + 0.5);
      uvec4 _cw1 = uvec4(texelFetch(s_lineColor, ivec2(ccRow, 3), 0) * 255.0 + 0.5);
      int _cs0 = int(_cw0.r) | (int(_cw0.g) << 8);
      int _ce0 = int(_cw0.b) | ((int(_cw0.a) & 0x7F) << 8);
      int _cs1 = int(_cw1.r) | (int(_cw1.g) << 8);
      int _ce1 = int(_cw1.b) | ((int(_cw1.a) & 0x7F) << 8);
      int _cwv = 0;
      if ((_cw0.a & 0x80u) != 0u && _cwx >= _cs0 && _cwx < _ce0) { _cwv |= 1; }
      if ((_cw1.a & 0x80u) != 0u && _cwx >= _cs1 && _cwx < _ce1) { _cwv |= 2; }
      // KNOWN GAP: the sprite window (cc-window bit 2, WCTLD bits 12/13) is not
      // evaluated here. It is a per-pixel VDP1 signal (framebuffer B byte bit 6,
      // see Vdp1ComputeRasterizer encodeBByte) that the sprite decoder currently
      // discards rather than forwarding, so the compositor has no membership to
      // test. The host mirrors this by never putting bit 2 into u_ccWindow.x
      // (VIDVulkan.cpp), which keeps the AND/OR evaluation below self-consistent
      // instead of testing a bit that is always 0.
      if (ubo.u_ccWindow.z == 0) {
        _ccAllows = ((_cwv & ubo.u_ccWindow.x) == ubo.u_ccWindow.y);
      } else {
        _ccAllows = ((_cwv & ubo.u_ccWindow.x) != ubo.u_ccWindow.y);
      }
    }
    if (!_ccAllows) {
      topCcEnable = false;
      topRatio = 0x3F;
    }
  }

  // Second operand: another opaque layer, or the back screen (no color calc).
  ivec3 secondColor;
  bool secondCcEnable;
  int secondRatio;
  if (idx[1] >= 0) {
    uint secondAttr = attrAt(idx[1]);
    secondCcEnable = (secondAttr & (1u << 5)) != 0u;
    secondRatio = int((secondAttr >> 6) & 0x3Fu);
    secondColor = colorRGB(idx[1]);
  } else {
    secondCcEnable = false;
    secondRatio = 0x3F;
    secondColor = backColorForRow();
  }

  // Normal-path LNCL line color screen (titan.c:324-327, vidsoft canonical).
  // When the top opaque layer has the line color screen inserted (its LNCLEN
  // bit is set, carried per slice in u_lineScreenMask) the top is blended with
  // the per-row line color using the GLOBAL blend mode -- this is independent
  // of EXCCEN and of the per-layer cc-enable (vidsoft sets info.linescreen from
  // LNCLEN alone, vidsoft.c:1651). In extended mode the line color is folded
  // into the extended-second operand instead (figure 12.3), so this normal-path
  // blend only runs when EXCCEN is off. The line color consumes the color-calc
  // slot, so the top is then marked opaque (ratio 0x3F / cc off) and the
  // top<->second stage below is naturally suppressed -- exactly as titan's
  // blend result (alpha 0x3F, trans bit clear) suppresses its later blend.
  if (exccEnableRow == 0 && topCcEnable &&
      (lineScreenMaskRow & (1 << idx[0])) != 0) {
    ivec3 lc = lineColorForRow();
    if (ubo.u_colorCalcMode == 1) {
      topColor = blendAdd(topColor, lc);              // TitanBlendPixelsAdd
    } else if (ubo.u_ratioFromSecond != 0) {
      // TitanBlendPixelsBottom: ratio from the line color (CCRLB), gated by the
      // top trans bit (cc-enable). A non-cc top passes through unchanged.
      if (topCcEnable) {
        topColor = blendRatio(topColor, lc, ubo.u_lineColorAlpha);
      }
    } else {
      // TitanBlendPixelsTop: ratio from the top layer's own cc ratio. Identity
      // when topRatio == 0x3F (top not color-calc'd).
      topColor = blendRatio(topColor, lc, topRatio);
    }
    topRatio = 0x3F;
    topCcEnable = false;
  }

  // Extended color calc: fold the priority-second / third (and line color) into
  // the extended second per the SW reference T_MixIt.
  if (exccEnableRow != 0) {
    ivec3 thirdColor = ivec3(0);
    if (idx[2] >= 0) {
      thirdColor = colorRGB(idx[2]);
    }
    ivec3 lineColor = lineColorForRow();
    // Per-slice direct-color (non-palette) flags. The SW reference reads the actual
    // PIX_ISRGB of each image; a missing layer (idx < 0) is palette (cannot be
    // an RGB operand). These match T_MixIt's pix3/pix4 ISRGB tests.
    bool secondRGB =
        (idx[1] >= 0) && ((ubo.u_paletteFormatMask & (1 << idx[1])) == 0);
    bool thirdRGB =
        (idx[2] >= 0) && ((ubo.u_paletteFormatMask & (1 << idx[2])) == 0);
    // Line color insertion is per-pixel gated on the TOP layer carrying the line
    // color screen (its LNCLEN bit), mirroring the SW reference's `pix & PIX_LCE_SHIFT`
    // gate -- not just the global LCCLEN flag. Without this a top layer that has
    // no line color screen would still get the line color folded in.
    bool lineInsert =
        (lineColorInsertedRow != 0) &&
        ((lineScreenMaskRow & (1 << idx[0])) != 0);
    secondColor = buildExtendedSecond(secondColor, thirdColor,
                                      secondCcEnable, secondRGB, thirdRGB,
                                      lineInsert, lineColor,
                                      ubo.u_cramMode == 0);
    // the SW reference T_MixIt: when the line color is inserted it BECOMES pix2, so under
    // CCRTMD=Second the ratio is the line color's (LineColorCCRatio / CCRLB), not
    // the priority-second's. The priority-second has shifted to pix3 and no longer
    // supplies the blend ratio.
    if (lineInsert) {
      secondRatio = ubo.u_lineColorAlpha;
    }
  }

  // Color gradation / blur (G5, BOKEN, CCCTL bit 15). When enabled and the top
  // or second layer is the blur-source slice, the second operand color is
  // replaced by a horizontal 3-tap blur of that slice: blurcake = avg(avg(x-2,
  // x-1), x), the causal running blur of the reference renderer. Tap spacing is
  // one Saturn dot (1/u_vwidth in uv). Mutually exclusive with EXCC (host clears
  // EXCCEN when gradation is active).
  if (ubo.u_gradEnable != 0 && ubo.u_gradSlice >= 0 &&
      (idx[0] == ubo.u_gradSlice || idx[1] == ubo.u_gradSlice)) {
    float dx = (ubo.u_vwidth > 0) ? (1.0 / float(ubo.u_vwidth)) : 0.0;
    ivec3 g0 = colorRGBAt(ubo.u_gradSlice, vec2(v_uv.x,          v_uv.y));
    ivec3 g1 = colorRGBAt(ubo.u_gradSlice, vec2(v_uv.x - dx,     v_uv.y));
    ivec3 g2 = colorRGBAt(ubo.u_gradSlice, vec2(v_uv.x - 2.0*dx, v_uv.y));
    secondColor = (((g2 + g1) >> 1) + g0) >> 1;
  }

  // Hi-res CRAM mode 1/2 (the SW reference MIXIT_SPECIAL_HIRES_CRAM12): with a palette-
  // format second image the blend is suppressed -- second = top, so the result is
  // the top image. Mutually exclusive with extended cc (the host clears EXCCEN in
  // hi-res, mirroring the SW reference's `if (!(HRes & 0x6))` EXCC gate).
  if (ubo.u_hiresCram12 != 0 && idx[1] >= 0 &&
      (ubo.u_paletteFormatMask & (1 << idx[1])) != 0) {
    secondColor = topColor;
  }

  // Color calc window: applied earlier (right after the top layer is
  // resolved) by clearing topCcEnable/topRatio, so every cc consumer above --
  // LNCL insertion, extended fold, and this final stage -- was suppressed in
  // the windowed-out region. blendTopSecondStage passes the top through when
  // topCcEnable is false, so no separate gate is needed here.
  ivec3 result = blendTopSecondStage(topColor, topRatio, topCcEnable,
                                     secondColor, secondRatio);

  // Color offset (Sega VDP2 manual ch.13): added AFTER color calc, to the result
  // (= the top image), using the TOP layer's offset (precomputed per slice on the
  // host; 0 when that layer's CLOFEN bit is clear). This is why per-line layers
  // that zero their own draw-time color offset still darken here: the offset is a
  // post-composite, top-image operation, not a per-layer bake.
  result = clamp(result + ubo.u_layerColorOffset[idx[0]].rgb, ivec3(0), ivec3(255));

  // Shadow half-luminance (G4): applied last, after color calc + offset, when a
  // topmost shadow caster fell on a shadow-accepting layer below.
  if (shadowHalfLum) {
    result = result >> 1;
  }

  outColor = vec4(vec3(result) / 255.0, 1.0);
}
)S";

Vdp2Compositor::Vdp2Compositor(VIDVulkan* vulkan) : vulkan(vulkan) {}

Vdp2Compositor::~Vdp2Compositor() {
  release();
}

void Vdp2Compositor::setup() {
  if (setupDone) {
    return;
  }
  createDescriptors();
  setupDone = true;
}

void Vdp2Compositor::createVertexBuffer() {
  // Intentionally empty: the fullscreen triangle is generated from
  // gl_VertexIndex, so no vertex/index buffer is required.
}

void Vdp2Compositor::createDescriptors() {
  VkDevice device = vulkan->getDevice();

  // UBO buffers (one per round-robin frame slot).
  VkDeviceSize uboSize = sizeof(UniformBufferObject);
  for (int i = 0; i < kFrames; i++) {
    vulkan->createBuffer(uboSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uboBuffer[i], uboMemory[i]);
  }

  // Descriptor set layout: binding 0 UBO, 1 color array sampler, 2 attr array
  // sampler (uint), 3 line color sampler (LNCL, T-015).
  std::array<VkDescriptorSetLayoutBinding, 4> bindings = {};
  bindings[0].binding = 0;
  bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  bindings[0].descriptorCount = 1;
  bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[1].binding = 1;
  bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[1].descriptorCount = 1;
  bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[2].binding = 2;
  bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[2].descriptorCount = 1;
  bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  bindings[3].binding = 3;
  bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  bindings[3].descriptorCount = 1;
  bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
  layoutInfo.pBindings = bindings.data();
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout));

  // Descriptor pool.
  std::array<VkDescriptorPoolSize, 2> poolSizes = {};
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = kFrames;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = kFrames * 3;  // color + attr + line color

  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.maxSets = kFrames;
  VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

  for (int i = 0; i < kFrames; i++) {
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet[i]));
  }

  // Pipeline layout: one descriptor set + a vertex push constant carrying the
  // pre-rotation (cos, sin) for the fullscreen triangle (issue #22 landscape fix).
  VkPushConstantRange pcRange = {};
  pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  pcRange.offset = 0;
  pcRange.size = sizeof(float) * 2;

  VkPipelineLayoutCreateInfo plInfo = {};
  plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  plInfo.setLayoutCount = 1;
  plInfo.pSetLayouts = &descriptorSetLayout;
  plInfo.pushConstantRangeCount = 1;
  plInfo.pPushConstantRanges = &pcRange;
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &plInfo, nullptr, &pipelineLayout));
}

VkShaderModule Vdp2Compositor::compileGlsl(const char* code, int shaderKind, uint32_t cacheKey) {
  (void)cacheKey;
  VkDevice device = vulkan->getDevice();

  std::string header = "#version 450\n";
  std::string target = header + std::string(code);

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  options.SetOptimizationLevel(shaderc_optimization_level_performance);
  shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(
      target,
      static_cast<shaderc_shader_kind>(shaderKind),
      "vdp2_compositor",
      options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    char msg[512];
    snprintf(msg, sizeof(msg), "Vdp2Compositor shader compile failed: %s",
             result.GetErrorMessage().c_str());
    throw std::runtime_error(msg);
  }

  std::vector<uint32_t> data(result.cbegin(), result.cend());
  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = data.size() * sizeof(uint32_t);
  createInfo.pCode = data.data();
  VkShaderModule module = VK_NULL_HANDLE;
  VK_CHECK_RESULT(vkCreateShaderModule(device, &createInfo, nullptr, &module));
  return module;
}

void Vdp2Compositor::createPipeline(VkRenderPass targetRenderPass) {
  VkDevice device = vulkan->getDevice();

  if (vertModule == VK_NULL_HANDLE) {
    vertModule = compileGlsl(kVertSrc, shaderc_vertex_shader, 0);
  }
  if (fragModule == VK_NULL_HANDLE) {
    fragModule = compileGlsl(kFragSrc, shaderc_fragment_shader, 1);
  }

  VkPipelineShaderStageCreateInfo vertStage = {};
  vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertStage.module = vertModule;
  vertStage.pName = "main";

  VkPipelineShaderStageCreateInfo fragStage = {};
  fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragStage.module = fragModule;
  fragStage.pName = "main";

  VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

  // No vertex input: fullscreen triangle is generated from gl_VertexIndex.
  VkPipelineVertexInputStateCreateInfo vertexInput = {};
  vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // The compositor is the final color write; depth/blend off (it replaces the
  // legacy priority loop's accumulation entirely).
  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable = VK_FALSE;
  depthStencil.depthWriteEnable = VK_FALSE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_ALWAYS;

  VkPipelineColorBlendAttachmentState colorAttachment = {};
  colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorAttachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorAttachment;

  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState = {};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = 2;
  dynamicState.pDynamicStates = dynamicStates;

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = stages;
  pipelineInfo.pVertexInputState = &vertexInput;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.renderPass = targetRenderPass;
  pipelineInfo.subpass = 0;

  VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));
  pipelineRenderPass = targetRenderPass;
}

void Vdp2Compositor::invalidatePipeline() {
  if (pipeline != VK_NULL_HANDLE) {
    vkDestroyPipeline(vulkan->getDevice(), pipeline, nullptr);
    pipeline = VK_NULL_HANDLE;
  }
  pipelineRenderPass = VK_NULL_HANDLE;
}

void Vdp2Compositor::composite(VkCommandBuffer commandBuffer,
                               Vdp2GBuffer* gbuffer,
                               VkRenderPass targetRenderPass,
                               const VkViewport& viewport,
                               const VkRect2D& scissor,
                               uint32_t backColorRGB,
                               int colorCalcMode,
                               int ratioFromSecond,
                               int exccEnable,
                               int lineColorInserted,
                               VkImageView lineColorView,
                               VkSampler lineColorSampler,
                               int vheight,
                               int lineScreenMask,
                               int lineColorAlpha,
                               int cramMode,
                               int paletteFormatMask,
                               int hiresCram12,
                               int gradEnable,
                               int gradSlice,
                               int vwidth,
                               const int* layerColorOffsetRGB,
                               const int* backColorOffsetRGB,
                               int debugViewSlice,
                               int ccWinMask,
                               int ccWinFlag,
                               int ccWinMode,
                               int mosaicX,
                               int mosaicY,
                               int mosaicMask,
                               float rotCos,
                               float rotSin,
                               int backPerLine) {
  if (!setupDone || gbuffer == nullptr || !gbuffer->isAllocated()) {
    return;
  }

  VkDevice device = vulkan->getDevice();

  // (Re)build the pipeline if the target render pass changed.
  if (pipeline == VK_NULL_HANDLE || pipelineRenderPass != targetRenderPass) {
    invalidatePipeline();
    createPipeline(targetRenderPass);
  }

  // Update the UBO for this frame slot.
  UniformBufferObject ubo = {};
  ubo.backColor[0] = static_cast<float>((backColorRGB >> 16) & 0xFF) / 255.0f;
  ubo.backColor[1] = static_cast<float>((backColorRGB >> 8) & 0xFF) / 255.0f;
  ubo.backColor[2] = static_cast<float>(backColorRGB & 0xFF) / 255.0f;
  ubo.backColor[3] = 1.0f;
  ubo.colorCalcMode = colorCalcMode;
  ubo.ratioFromSecond = ratioFromSecond;
  ubo.exccEnable = exccEnable;
  ubo.lineColorInserted = lineColorInserted;
  ubo.vheight = vheight;
  ubo.debugViewSlice = debugViewSlice;
  ubo.lineScreenMask = lineScreenMask;
  ubo.lineColorAlpha = lineColorAlpha;
  ubo.cramMode = cramMode;
  ubo.paletteFormatMask = paletteFormatMask;
  ubo.hiresCram12 = hiresCram12;
  ubo.gradEnable = gradEnable;
  ubo.gradSlice = gradSlice;
  ubo.vwidth = vwidth;
  ubo.backPerLine = backPerLine;
  ubo.pad2 = 0;
  for (int s = 0; s < 6; s++) {
    ubo.layerColorOffset[s][0] = layerColorOffsetRGB ? layerColorOffsetRGB[s * 3 + 0] : 0;
    ubo.layerColorOffset[s][1] = layerColorOffsetRGB ? layerColorOffsetRGB[s * 3 + 1] : 0;
    ubo.layerColorOffset[s][2] = layerColorOffsetRGB ? layerColorOffsetRGB[s * 3 + 2] : 0;
    ubo.layerColorOffset[s][3] = 0;
  }
  ubo.backColorOffset[0] = backColorOffsetRGB ? backColorOffsetRGB[0] : 0;
  ubo.backColorOffset[1] = backColorOffsetRGB ? backColorOffsetRGB[1] : 0;
  ubo.backColorOffset[2] = backColorOffsetRGB ? backColorOffsetRGB[2] : 0;
  ubo.backColorOffset[3] = 0;
  ubo.ccWindow[0] = ccWinMask;
  ubo.ccWindow[1] = ccWinFlag;
  ubo.ccWindow[2] = ccWinMode;
  ubo.ccWindow[3] = 0;
  ubo.mosaic[0] = (mosaicX >= 1) ? mosaicX : 1;
  ubo.mosaic[1] = (mosaicY >= 1) ? mosaicY : 1;
  ubo.mosaic[2] = mosaicMask;
  ubo.mosaic[3] = 0;

  int fi = frameIndex;
  frameIndex = (frameIndex + 1) % kFrames;

  void* data = nullptr;
  vkMapMemory(device, uboMemory[fi], 0, sizeof(ubo), 0, &data);
  memcpy(data, &ubo, sizeof(ubo));
  vkUnmapMemory(device, uboMemory[fi]);

  // Update the descriptor set for this frame slot.
  VkDescriptorBufferInfo bufInfo = {};
  bufInfo.buffer = uboBuffer[fi];
  bufInfo.offset = 0;
  bufInfo.range = sizeof(ubo);

  VkDescriptorImageInfo colorInfo = {};
  colorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  colorInfo.imageView = gbuffer->getColorArrayView();
  colorInfo.sampler = gbuffer->getSampler();

  VkDescriptorImageInfo attrInfo = {};
  attrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  attrInfo.imageView = gbuffer->getAttrArrayView();
  attrInfo.sampler = gbuffer->getSampler();

  // Line color (LNCL). The caller must pass a valid 2D line color view/sampler
  // (the port's lineColor texture is created unconditionally at init); binding 3
  // is a sampler2D, so an array view must not be substituted here. The sampled
  // result is only consumed when u_lineColorInserted is set.
  if (lineColorView == VK_NULL_HANDLE || lineColorSampler == VK_NULL_HANDLE) {
    return;
  }
  VkDescriptorImageInfo lineInfo = {};
  lineInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  lineInfo.imageView = lineColorView;
  lineInfo.sampler = lineColorSampler;

  std::array<VkWriteDescriptorSet, 4> writes = {};
  writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[0].dstSet = descriptorSet[fi];
  writes[0].dstBinding = 0;
  writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writes[0].descriptorCount = 1;
  writes[0].pBufferInfo = &bufInfo;

  writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[1].dstSet = descriptorSet[fi];
  writes[1].dstBinding = 1;
  writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[1].descriptorCount = 1;
  writes[1].pImageInfo = &colorInfo;

  writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[2].dstSet = descriptorSet[fi];
  writes[2].dstBinding = 2;
  writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[2].descriptorCount = 1;
  writes[2].pImageInfo = &attrInfo;

  writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writes[3].dstSet = descriptorSet[fi];
  writes[3].dstBinding = 3;
  writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  writes[3].descriptorCount = 1;
  writes[3].pImageInfo = &lineInfo;

  vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

  // Draw the fullscreen triangle.
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  // Pre-rotation (issue #22): the vertex shader rotates the clip position by
  // (cos, sin) so the output matches the swapchain pre-transform / rotate_screen.
  float pushRot[2] = {rotCos, rotSin};
  vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(pushRot), pushRot);
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout,
                          0, 1, &descriptorSet[fi], 0, nullptr);
  vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void Vdp2Compositor::release() {
  VkDevice device = vulkan->getDevice();
  if (device == VK_NULL_HANDLE) {
    return;
  }

  invalidatePipeline();

  if (vertModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device, vertModule, nullptr);
    vertModule = VK_NULL_HANDLE;
  }
  if (fragModule != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device, fragModule, nullptr);
    fragModule = VK_NULL_HANDLE;
  }
  if (pipelineLayout != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    pipelineLayout = VK_NULL_HANDLE;
  }
  if (descriptorPool != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    descriptorPool = VK_NULL_HANDLE;
  }
  if (descriptorSetLayout != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    descriptorSetLayout = VK_NULL_HANDLE;
  }
  for (int i = 0; i < kFrames; i++) {
    if (uboBuffer[i] != VK_NULL_HANDLE) {
      vkDestroyBuffer(device, uboBuffer[i], nullptr);
      uboBuffer[i] = VK_NULL_HANDLE;
    }
    if (uboMemory[i] != VK_NULL_HANDLE) {
      vkFreeMemory(device, uboMemory[i], nullptr);
      uboMemory[i] = VK_NULL_HANDLE;
    }
  }
  setupDone = false;
}
