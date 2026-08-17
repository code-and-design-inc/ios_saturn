// Copyright 2026 devMiyax
//
// VDP2 reference oracle (issue #22, task T-001).
//
// Pure, side-effect-free host port of the bit-accurate software renderer's
// VDP2 compositing logic. The software renderer (`vidsoft.c` +
// `titan/titan.c`) is the reference for the new shader-based VDP2 color
// calculation pipeline; these functions mirror its behaviour so unit tests
// (UT-001..UT-014 etc. in docs/feature/issue-22/02-tests.md) can compare the
// future host-port / GLSL logic against a known-good baseline.
//
// SOURCE OF TRUTH (do not diverge without updating both sides):
//   - CRAM color fetch ............ vidsoft.c  Vdp2ColorRamGetColorSoft()
//   - per-layer color calc ratio .. vidsoft.c  Vdp2DrawNBG0..3 / RBG0 / sprite
//   - priority select + blend ..... titan/titan.c  TitanDigPixel()
//   - blend functions ............. titan/titan.c  TitanBlendPixels{Top,Bottom,Add}
//   - line color screen ratio ..... vidsoft.c  VIDSoftVdp2DrawLineColorScreen()
//
// Pixel encoding (little-endian, non-RGB555 build = the default desktop build,
// see COLSAT2YAB32 in vidsoft.c and TitanGet* in titan.c):
//   bits  0.. 7  : blue
//   bits  8..15  : green
//   bits 16..23  : red
//   bits 24..29  : color-calc ratio (alpha, 0..0x3F)  -> TitanGetAlpha
//   bit  31      : color-calc-enable / "trans" bit (0x80000000) for ADD/BOTTOM
//                  blend selection.  Set by `info.alpha |= 0x80` in vidsoft.c.
//
// IMPORTANT (finding for the orchestrator / T-002+): the software renderer's
// titan compositor is a TWO-layer model (top + one bottom layer, plus the back
// screen). It does NOT implement the design doc's "extended color calc" that
// reaches the 3rd/4th layer independently. See the note on selectTopTwo() and
// the report. The oracle therefore provides:
//   (a) faithful 2-layer titan behaviour (basic + normal color calc) for
//       parity testing of MVP stage 1/2, and
//   (b) a generalized N-layer priority sort/select helper so stage 3 (extended
//       color calc, UT-008) has a documented selection order to build on.
//
// ASCII-only comments (CLAUDE.md rule: MSVC CP932 -> C4819/C2065).

#ifndef VDP2_COLOR_ORACLE_H
#define VDP2_COLOR_ORACLE_H

#include <cstdint>
#include <array>

namespace vdp2oracle {

// ---------------------------------------------------------------------------
// Pixel bit layout (mirrors titan.c TitanGet*/TitanCreatePixel, little-endian
// non-RGB555 build).
// ---------------------------------------------------------------------------

inline uint8_t getBlue(uint32_t pixel)  { return static_cast<uint8_t>(pixel & 0xFF); }
inline uint8_t getGreen(uint32_t pixel) { return static_cast<uint8_t>((pixel >> 8) & 0xFF); }
inline uint8_t getRed(uint32_t pixel)   { return static_cast<uint8_t>((pixel >> 16) & 0xFF); }
inline uint8_t getAlpha(uint32_t pixel) { return static_cast<uint8_t>((pixel >> 24) & 0x3F); }

inline uint32_t createPixel(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue) {
    return (static_cast<uint32_t>(alpha) << 24)
         | (static_cast<uint32_t>(red)   << 16)
         | (static_cast<uint32_t>(green) << 8)
         | static_cast<uint32_t>(blue);
}

// ---------------------------------------------------------------------------
// CRAM color fetch. Port of vidsoft.c Vdp2ColorRamGetColorSoft().
//
// colorMode mirrors Vdp2Internal.ColorMode:
//   0 -> RGB555, 1024 colors (CRAM 0x000-0x7FF, word entries)
//   1 -> RGB555, 2048 colors (CRAM word entries)
//   2 -> RGB888, 1024 colors (CRAM long entries)
//
// `cram` points at the raw CRAM bytes; both word/long reads mask the address
// with 0xFFF exactly like the original (T2ReadWord/T2ReadLong). The returned
// value is an 0x00RRGGBB color with the original CRAM MSB preserved at bit 24+
// (bit 31) for special color calculation mode 3 (modes 0/1 only), matching the
// `((tmp & 0x8000) << 16)` term in the original.
//
// NOTE on endianness: T2ReadWord/T2ReadLong on a little-endian build read in
// native order from the byte-swapped CRAM image. For host-port parity tests,
// callers should populate `cram` with the same byte order the test exercises;
// readWordT2/readLongT2 below replicate the little-endian native read used by
// the default desktop build (USE_OPENGL not RGB555).
// ---------------------------------------------------------------------------

inline uint16_t readWordT2(const uint8_t* cram, uint32_t addr) {
    // T2ReadWord on little-endian: native u16 at byte address.
    return static_cast<uint16_t>(cram[addr] | (cram[addr + 1] << 8));
}

inline uint32_t readLongT2(const uint8_t* cram, uint32_t addr) {
    return static_cast<uint32_t>(cram[addr])
         | (static_cast<uint32_t>(cram[addr + 1]) << 8)
         | (static_cast<uint32_t>(cram[addr + 2]) << 16)
         | (static_cast<uint32_t>(cram[addr + 3]) << 24);
}

inline uint32_t cramGetColor(uint32_t colorMode, uint32_t addr, const uint8_t* cram) {
    switch (colorMode) {
        case 0:
        case 1: {
            uint32_t a = addr << 1;
            uint32_t tmp = readWordT2(cram, a & 0xFFF);
            return (((tmp & 0x1F) << 3) | ((tmp & 0x03E0) << 6) | ((tmp & 0x7C00) << 9))
                 | ((tmp & 0x8000) << 16);
        }
        case 2: {
            uint32_t a = addr << 2;
            return readLongT2(cram, a & 0xFFF);
        }
        default:
            break;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Per-layer color calc ratio decode. Port of the repeated
//   if (CCCTL & enableMask) info.alpha = ((~CCRxx & mask) shift) + 1; else 0x3F;
//   if ((CCCTL & topMask) == topMask) info.alpha |= 0x80; ...
// blocks in vidsoft.c (Vdp2DrawNBG0..3 / RBG0).
//
// `ratioReg`   : the relevant CCRNA/CCRNB/CCRR register value.
// `ratioField` : which 5-bit field (0 = bits 0-4, 1 = bits 8-12).
// `cctl`       : CCCTL register.
// `enableBit`  : the per-screen color-calc-enable bit in CCCTL (e.g. 0x001 NBG0).
// `extBit`     : the "color calc on second screen" gating bit (e.g. 0x200) for
//                the 0x80 MSB; vidsoft sets 0x80 when (CCCTL & (extBit|enableBit))
//                == (extBit|enableBit) OR (CCCTL & (0x100|enableBit)) == ...
//
// Returns the 8-bit alpha field stored into the pixel: bits 0-5 = ratio
// (0..0x3F), bit 7 (0x80) = color-calc / "trans" flag for ADD/BOTTOM blend.
//
// Reference (Vdp2DrawNBG0, vidsoft.c:1639):
//   if (regs->CCCTL & 0x201) info.alpha = ((~regs->CCRNA & 0x1F) << 1) + 1;
//   else info.alpha = 0x3F;
//   if ((regs->CCCTL & 0x201) == 0x201) info.alpha |= 0x80;
//   else if ((regs->CCCTL & 0x101) == 0x101) info.alpha |= 0x80;
// Here the "0x201" test is (extBit | enableBit), "0x101" is (0x100 | enableBit).
// ---------------------------------------------------------------------------

inline uint8_t decodeLayerAlpha(uint32_t cctl,
                                uint32_t ratioReg,
                                int ratioField,
                                uint32_t enableBit,
                                uint32_t extBit) {
    uint8_t alpha;
    if (cctl & (extBit | enableBit)) {
        uint32_t ratio5;
        if (ratioField == 0) {
            ratio5 = (~ratioReg) & 0x1F;
            alpha = static_cast<uint8_t>((ratio5 << 1) + 1);
        } else {
            // vidsoft: ((~reg & 0x1F00) >> 7) + 1
            ratio5 = ((~ratioReg) & 0x1F00) >> 8;
            alpha = static_cast<uint8_t>((ratio5 << 1) + 1);
        }
    } else {
        alpha = 0x3F;
    }
    if ((cctl & (extBit | enableBit)) == (extBit | enableBit)) {
        alpha |= 0x80;
    } else if ((cctl & (0x100u | enableBit)) == (0x100u | enableBit)) {
        alpha |= 0x80;
    }
    return alpha;
}

// Sprite color calc ratio table. Port of vidsoft.c:3588 (colorcalctable[8]).
// `ccrs` is an array of the 4 sprite ratio registers {CCRSA,CCRSB,CCRSC,CCRSD}.
// Produces 8 entries (CCRTMD-independent), each ((~reg & 0x1F) << 1) + 1 etc.
inline std::array<uint8_t, 8> decodeSpriteColorCalcTable(const std::array<uint16_t, 4>& ccrs) {
    std::array<uint8_t, 8> t{};
    t[0] = static_cast<uint8_t>(((~ccrs[0] & 0x1F) << 1) + 1);
    t[1] = static_cast<uint8_t>(((~ccrs[0] >> 7) & 0x3E) + 1);
    t[2] = static_cast<uint8_t>(((~ccrs[1] & 0x1F) << 1) + 1);
    t[3] = static_cast<uint8_t>(((~ccrs[1] >> 7) & 0x3E) + 1);
    t[4] = static_cast<uint8_t>(((~ccrs[2] & 0x1F) << 1) + 1);
    t[5] = static_cast<uint8_t>(((~ccrs[2] >> 7) & 0x3E) + 1);
    t[6] = static_cast<uint8_t>(((~ccrs[3] & 0x1F) << 1) + 1);
    t[7] = static_cast<uint8_t>(((~ccrs[3] >> 7) & 0x3E) + 1);
    return t;
}

// ---------------------------------------------------------------------------
// Blend functions. Direct port of titan.c.
// ---------------------------------------------------------------------------

// the SW reference ss/vdp2_render.cpp T_MixIt ratio blend (32-level, the authoritative
// reference). fore_ratio = (rawCCR & 0x1F) ^ 0x1F == ratio5; the gbuffer/titan
// alpha field is (ratio5 << 1) + 1 for layers and (lineRatio5 << 1) for the line
// color, so `getAlpha(...) >> 1` recovers fore_ratio for both. The blend is
// (fore*fore_ratio + sec*sec_ratio) >> 5 with sec_ratio = 0x20 - fore_ratio.
inline uint32_t mixRatio(uint32_t fore, uint32_t sec, int foreRatio) {
    int fr = foreRatio;
    int sr = 0x20 - fr;
    uint8_t r = static_cast<uint8_t>((getRed(fore)   * fr + getRed(sec)   * sr) >> 5);
    uint8_t g = static_cast<uint8_t>((getGreen(fore) * fr + getGreen(sec) * sr) >> 5);
    uint8_t b = static_cast<uint8_t>((getBlue(fore)  * fr + getBlue(sec)  * sr) >> 5);
    return createPixel(0, r, g, b);
}

// TitanBlendPixelsTop equivalent: ratio comes from the TOP pixel's alpha field.
inline uint32_t blendTop(uint32_t top, uint32_t bottom) {
    uint32_t m = mixRatio(top, bottom, getAlpha(top) >> 1);
    return createPixel(0x3F, getRed(m), getGreen(m), getBlue(m));
}

// TitanBlendPixelsBottom equivalent: ratio comes from the BOTTOM pixel's alpha;
// short-circuits when top has no trans bit (0x80000000).
inline uint32_t blendBottom(uint32_t top, uint32_t bottom) {
    if ((top & 0x80000000u) == 0) return top;
    uint32_t m = mixRatio(top, bottom, getAlpha(bottom) >> 1);
    return createPixel(getAlpha(top), getRed(m), getGreen(m), getBlue(m));
}

// titan.c TitanBlendPixelsAdd: saturating add, ratio ignored.
inline uint32_t blendAdd(uint32_t top, uint32_t bottom) {
    uint32_t r = static_cast<uint32_t>(getRed(top))   + getRed(bottom);
    if (r > 0xFF) r = 0xFF;
    uint32_t g = static_cast<uint32_t>(getGreen(top)) + getGreen(bottom);
    if (g > 0xFF) g = 0xFF;
    uint32_t b = static_cast<uint32_t>(getBlue(top))  + getBlue(bottom);
    if (b > 0xFF) b = 0xFF;
    return createPixel(0x3F,
                       static_cast<uint8_t>(r),
                       static_cast<uint8_t>(g),
                       static_cast<uint8_t>(b));
}

// Blend mode selector (mirrors titan.c TitanSetBlendingMode).
enum class BlendMode {
    Top = 0,     // TITAN_BLEND_TOP    -> blendTop  + trans = alpha < 0x3F
    Bottom = 1,  // TITAN_BLEND_BOTTOM -> blendBottom + trans = bit 31
    Add = 2,     // TITAN_BLEND_ADD    -> blendAdd  + trans = bit 31
};

// titan.c trans functions: decide whether color calc runs at all for `top`.
inline bool transTest(BlendMode mode, uint32_t pixel) {
    if (mode == BlendMode::Top) {
        return getAlpha(pixel) < 0x3F;            // TitanTransAlpha
    }
    return (pixel & 0x80000000u) != 0;            // TitanTransBit
}

inline uint32_t applyBlend(BlendMode mode, uint32_t top, uint32_t bottom) {
    switch (mode) {
        case BlendMode::Top:    return blendTop(top, bottom);
        case BlendMode::Bottom: return blendBottom(top, bottom);
        case BlendMode::Add:    return blendAdd(top, bottom);
    }
    return top;
}

// ---------------------------------------------------------------------------
// Priority selection.
// ---------------------------------------------------------------------------

// Logical layer ids mirroring titan.h. Index into the per-pixel layer arrays.
enum LayerId {
    kNBG3 = 0,
    kNBG2 = 1,
    kNBG1 = 2,
    kNBG0 = 3,
    kRBG0 = 4,
    kSprite = 5,
    kLayerCount = 6,
    kBack = -1,
};

// One layer's per-pixel contribution.
struct LayerPixel {
    uint32_t pixel = 0;   // encoded ARGB (alpha = cc ratio, bit31 = cc flag)
    uint8_t priority = 0; // 0 = transparent / not drawn (titan: priority 0 skipped)
    uint8_t linescreen = 0;
    uint8_t shadowType = 0;     // 0 none / 1 TITAN_NORMAL_SHADOW / 2 TITAN_MSB_SHADOW
    uint8_t shadowEnabled = 0;
};

// Result of the top-two priority pick (titan.c TitanDigPixel pixel_stack[2]).
struct TopTwo {
    LayerPixel stack[2];
    bool usedBack = false; // true when stack[1] (or stack[0]) is the back screen
};

// Saturn VDP2 hardware priority is a 3-bit value (0..7); priority 0 means the
// layer is not drawn (transparent). The G-buffer attr field reserves 5 bits
// (see packAttr below) so an out-of-range value (8..31) can be stored; the
// compositor must clamp it to the hardware-valid range so the sort/select is
// deterministic and never reads outside the priority loop (UT-E02). Clamping to
// kMaxPriority (7) keeps an over-large value as "topmost" rather than silently
// dropping it (which would happen if the descending loop started at 7 and the
// value were 8+). GLSL counterpart (T-008) must apply the same min(p, 7u).
constexpr int kMaxPriority = 7;

inline uint8_t clampPriority(int priority) {
    if (priority < 0) return 0;
    if (priority > kMaxPriority) return static_cast<uint8_t>(kMaxPriority);
    return static_cast<uint8_t>(priority);
}

// Port of TitanDigPixel's sort loop (titan.c:294-322).
//
// Iterates priority 7..1 (descending). Within each priority, scans layers from
// kSprite down to kNBG3 (this is the index tiebreak: higher LayerId wins on a
// tie). Collects up to two non-transparent layers (priority != target skips the
// layer; a layer with priority==0 is implicitly transparent because it never
// equals a positive target priority). If fewer than two opaque layers are
// found, the back screen fills the remaining slot(s).
//
// `layers` is indexed by LayerId (0..5). `back` is the back-screen pixel.
inline TopTwo selectTopTwo(const std::array<LayerPixel, kLayerCount>& layers,
                           const LayerPixel& back) {
    TopTwo out;
    int pos = 0;
    for (int priority = kMaxPriority; priority > 0 && pos < 2; --priority) {
        for (int which = kSprite; which >= 0 && pos < 2; --which) {
            if (clampPriority(layers[which].priority) == priority) {
                out.stack[pos] = layers[which];
                ++pos;
            }
        }
    }
    if (pos < 2) {
        out.stack[pos] = back;
        out.usedBack = true;
    }
    return out;
}

// Generalized N-layer priority select for the new design (stage 3 / extended
// color calc, UT-004). titan only keeps two layers, but the design's extended
// color calc needs top..fourth. This returns layer indices sorted by priority
// descending using the SAME tiebreak as titan (higher LayerId first), skipping
// transparent (priority == 0) layers. `out[k]` = LayerId of the k-th layer, or
// kBack sentinel (-1) once layers are exhausted. `count` = number of opaque
// layers found (back screen not included).
//
// Each layer's priority is clamped to the hardware-valid range (0..7) before
// comparison so a malformed / out-of-range attr value cannot make the layer
// fall outside the descending loop and disappear (UT-E02). The descending
// `priority` loop spans kMaxPriority..1, matching clampPriority's upper bound.
// GLSL counterpart (T-008): same nested loop, same min(p,7u) clamp, same
// inner scan from kSprite down to kNBG3 for the index tiebreak (UT-B03).
inline void selectSortedLayers(const std::array<LayerPixel, kLayerCount>& layers,
                               std::array<int, kLayerCount>& out,
                               int& count) {
    count = 0;
    for (int i = 0; i < kLayerCount; ++i) out[i] = kBack;
    for (int priority = kMaxPriority; priority > 0; --priority) {
        for (int which = kSprite; which >= 0; --which) {
            if (clampPriority(layers[which].priority) == priority) {
                out[count++] = which;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Full per-pixel composite. Port of titan.c TitanDigPixel() including
// linescreen, shadow and color calc. `lineColorForRow` is the line-screen color
// (titan.c indexes linescreen[stack[0].linescreen][y]); pass the resolved color
// for the current row.
//
// This is the authoritative basic + normal-color-calc oracle (MVP stages 1/2).
// Shadow handling is included for completeness/parity but is a phase2 feature
// per the design scope.
// ---------------------------------------------------------------------------
inline uint32_t composite(const std::array<LayerPixel, kLayerCount>& layers,
                          const LayerPixel& back,
                          BlendMode mode,
                          uint32_t lineColorForRow,
                          uint16_t spctl) {
    TopTwo t = selectTopTwo(layers, back);
    LayerPixel s0 = t.stack[0];
    LayerPixel s1 = t.stack[1];

    if (s0.linescreen) {
        s0.pixel = applyBlend(mode, s0.pixel, lineColorForRow);
    }

    constexpr uint8_t kMsbShadow = 2;    // TITAN_MSB_SHADOW
    constexpr uint8_t kNormalShadow = 1; // TITAN_NORMAL_SHADOW

    if ((s0.shadowType == kMsbShadow) && ((s0.pixel & 0xFFFFFF) == 0)) {
        if (s1.shadowEnabled) {
            s0.pixel = blendTop(0x20000000u, s1.pixel);
        } else {
            s0.pixel = s1.pixel;
        }
    } else if (s0.shadowType == kMsbShadow && ((s0.pixel & 0xFFFFFF) != 0)) {
        if (transTest(mode, s0.pixel)) {
            s0.pixel = applyBlend(mode, s0.pixel, s1.pixel);
        }
        if (!(spctl & 0x10)) {
            s0.pixel = blendTop(0x20000000u, s0.pixel);
        }
    } else if (s0.shadowType == kNormalShadow) {
        if (s1.shadowEnabled) {
            s0.pixel = blendTop(0x20000000u, s1.pixel);
        } else {
            s0.pixel = s1.pixel;
        }
    } else {
        if (transTest(mode, s0.pixel)) {
            s0.pixel = applyBlend(mode, s0.pixel, s1.pixel);
        }
    }

    return s0.pixel;
}

// ---------------------------------------------------------------------------
// Line color screen ratio decode. Port of vidsoft.c:1516
//   alpha = (Vdp2Regs->CCRLB & 0x1f) << 1;
// (Note: line color does NOT add +1, unlike the layer ratios.)
// ---------------------------------------------------------------------------
inline uint8_t decodeLineColorAlpha(uint16_t ccrlb) {
    return static_cast<uint8_t>((ccrlb & 0x1F) << 1);
}

// ---------------------------------------------------------------------------
// attr pack/unpack (UT-003). NOT present in vidsoft; this is the new design's
// G-buffer attribute encoding (R32_UINT). Defined here so the host-port and the
// GLSL packing share one definition. Layout:
//   bits  0.. 4 : priority    (0..31)
//   bit   5     : ccEnable
//   bits  6..11 : ccRatio     (0..0x3F)
//   bit   12    : transparent
//   bit   13    : doShadow  (this pixel casts a shadow on the layer below)
//   bit   14    : shadowEn  (this layer accepts shadows; from SDCTL)
// Spare bits 15..31 reserved (zero).
// ---------------------------------------------------------------------------
struct Attr {
    uint8_t priority = 0;     // 0..31
    bool ccEnable = false;
    uint8_t ccRatio = 0;      // 0..0x3F
    bool transparent = false;
    bool doShadow = false;    // shadow caster (sprite MSB / shadow pixel)
    bool shadowEn = false;    // accepts shadow (SDCTL bit set for this layer)
};

inline uint32_t packAttr(const Attr& a) {
    return (static_cast<uint32_t>(a.priority & 0x1F))
         | (a.ccEnable ? (1u << 5) : 0u)
         | (static_cast<uint32_t>(a.ccRatio & 0x3F) << 6)
         | (a.transparent ? (1u << 12) : 0u)
         | (a.doShadow ? (1u << 13) : 0u)
         | (a.shadowEn ? (1u << 14) : 0u);
}

inline Attr unpackAttr(uint32_t v) {
    Attr a;
    a.priority    = static_cast<uint8_t>(v & 0x1F);
    a.ccEnable    = (v & (1u << 5)) != 0;
    a.ccRatio     = static_cast<uint8_t>((v >> 6) & 0x3F);
    a.transparent = (v & (1u << 12)) != 0;
    a.doShadow    = (v & (1u << 13)) != 0;
    a.shadowEn    = (v & (1u << 14)) != 0;
    return a;
}

// ---------------------------------------------------------------------------
// Shadow resolution + half-luminance (G4). Mirrors the reference renderer's
// PIX_DOSHAD handling: when the topmost opaque layer is a shadow caster
// (doShadow), it is dropped and the layers below are re-selected; if the new
// topmost accepts the shadow (shadowEn, from SDCTL) the final composite is
// darkened by half luminance. (Self-shadow / MSB-vs-color-shadow are not
// distinguishable from the VDP1 framebuffer the sprite decoder samples, so only
// the cast-shadow-onto-layer-below case is modelled.)
// ---------------------------------------------------------------------------

// Per-channel half-luminance: (c >> 1) per RGB channel.
inline uint32_t halfLuminance(uint32_t rgb) {
    return createPixel(0, static_cast<uint8_t>(getRed(rgb) >> 1),
                       static_cast<uint8_t>(getGreen(rgb) >> 1),
                       static_cast<uint8_t>(getBlue(rgb) >> 1));
}

// Select up to four opaque layers by priority (topmost first), optionally
// skipping one slice. out[k] = slice of the k-th layer, or -1.
inline void selectOpaqueLayers(const std::array<uint32_t, kLayerCount>& attr,
                               int skipSlice, int out[4]) {
    out[0] = out[1] = out[2] = out[3] = -1;
    int found = 0;
    for (int priority = kMaxPriority; priority > 0 && found < 4; --priority) {
        for (int which = kSprite; which >= 0 && found < 4; --which) {
            if (which == skipSlice) continue;
            Attr a = unpackAttr(attr[which]);
            if (a.transparent) continue;
            if (clampPriority(a.priority) == 0) continue;
            if (clampPriority(a.priority) != priority) continue;
            out[found++] = which;
        }
    }
}

// Resolve the displayed layer stack accounting for a topmost shadow caster.
// idxOut receives the layers to composite (shadow caster dropped); halfLum is set
// when the new topmost accepts the shadow.
inline void selectLayersShadow(const std::array<uint32_t, kLayerCount>& attr,
                               int idxOut[4], bool& halfLum) {
    halfLum = false;
    selectOpaqueLayers(attr, -1, idxOut);
    if (idxOut[0] >= 0 && unpackAttr(attr[idxOut[0]]).doShadow) {
        int shadowSlice = idxOut[0];
        selectOpaqueLayers(attr, shadowSlice, idxOut);
        if (idxOut[0] >= 0 && unpackAttr(attr[idxOut[0]]).shadowEn) {
            halfLum = true;
        }
    }
}

// ---------------------------------------------------------------------------
// Basic-pattern composite (issue #22 task T-008, MVP stage 1, NO color calc).
//
// This is the host-port reference the new GLSL compositor (Vdp2Compositor,
// vdp2_basic_compositor.frag) reproduces 1:1 for the basic pattern: at each
// pixel, pick the color of the highest-priority opaque layer; if no layer is
// opaque, output the back-screen color. There is no blending, no line color,
// no shadow (those belong to stages 2/3 / phase2).
//
// `attr` is the per-layer packed G-buffer attribute word (see packAttr): bits
// 0-4 priority, bit 5 ccEnable, bits 6-11 ccRatio, bit 12 transparent. A layer
// is opaque when its clamped priority is > 0 AND its transparent bit is clear.
// `color` is the per-layer RGBA8 color (alpha ignored here). The selection uses
// the SAME descending-priority / LayerId-tiebreak loop as selectSortedLayers
// (titan inner scan from kSprite down to kNBG3), so the GLSL counterpart MUST
// mirror it: loop priority 7..1, inner which kSprite..kNBG3, clamp priority to
// [0,7], skip transparent.
//
// Returns the chosen RGB color (0x00RRGGBB); callers compare via
// getRed/getGreen/getBlue.
// ---------------------------------------------------------------------------
inline uint32_t compositeBasic(const std::array<uint32_t, kLayerCount>& color,
                               const std::array<uint32_t, kLayerCount>& attr,
                               uint32_t backColor) {
    for (int priority = kMaxPriority; priority > 0; --priority) {
        for (int which = kSprite; which >= 0; --which) {
            Attr a = unpackAttr(attr[which]);
            if (a.transparent) continue;
            if (clampPriority(a.priority) == 0) continue;
            if (clampPriority(a.priority) == priority) {
                return color[which] & 0x00FFFFFFu;
            }
        }
    }
    return backColor & 0x00FFFFFFu;
}

// ---------------------------------------------------------------------------
// Normal color-calc composite (issue #22 task T-012, MVP stage 2). Extends the
// basic pattern with the titan TWO-layer color calculation: at each pixel the
// top and second opaque layers are selected (same descending-priority /
// LayerId-tiebreak loop as compositeBasic / selectSortedLayers), then if color
// calc applies the two colors are blended.
//
// This is the authoritative host-port reference the GLSL compositor reproduces
// 1:1 for normal color calc (UT-006/UT-007/UT-B04/UT-B05). It is expressed in
// terms of the SAME per-layer attr fields the G-buffer carries (ccEnable bit 5,
// ccRatio bits 6..11) instead of the titan u32 pixel encoding, because that is
// what the compositor has per-pixel. The result is identical to the full titan
// `composite()` for the 2-layer (basic + normal) case: see below for the field
// mapping.
//
// Field mapping (attr -> titan pixel encoding consumed by blendTop/Bottom/Add):
//   - ccRatio (0..0x3F)  == titan pixel alpha (bits 24..29). blendTop uses the
//     TOP layer's ratio; blendBottom uses the SECOND layer's ratio (CCRTMD).
//   - ccEnable            == titan pixel bit 31 ("trans" flag) for the
//     BOTTOM/ADD blend modes. vidsoft sets the 0x80 flag exactly when CCRTMD or
//     CCMD pairs with the layer enable bit, which is the same condition that
//     makes ccEnable true; so ccEnable is the faithful per-pixel proxy for the
//     bit-31 trans flag in those modes (see Vdp2ColorCalcState.h decodeLayer).
//
// `mode` is the global blend mode (CCMD: Ratio -> Top/Bottom by CCRTMD; Add).
// `ratioSource` is CCRTMD (Top = ratio from top layer, Second = ratio from the
// second layer). `backColor` fills the second slot when only one opaque layer
// exists (back screen has no color calc -> ccEnable false, ratio 0x3F).
//
// Color-calc gating mirrors titan transTest():
//   - Ratio + Top  (TITAN_BLEND_TOP):    blend iff topRatio < 0x3F
//   - Ratio + Second (TITAN_BLEND_BOTTOM): blend iff top ccEnable (bit 31)
//   - Add (TITAN_BLEND_ADD):             blend iff top ccEnable (bit 31)
// When the top layer's color calc does not apply, the top color passes through
// unchanged (basic pattern). Returns 0x00RRGGBB.
// ---------------------------------------------------------------------------
// Normal-path LNCL line color screen (T-015): titan.c:324 blends the TOP opaque
// layer with the per-row line color whenever that layer has the line color
// screen inserted (LNCLEN selects it; vidsoft.c:1651). `lineScreenMask` carries
// one bit per LayerId slice; `lineColor` is the resolved 0x00RRGGBB line color
// for the current row; `lineColorAlpha` is (CCRLB & 0x1F) << 1 (vidsoft.c:1516),
// used only by the CCRTMD=second (Bottom) blend. Defaults keep the pre-T-015
// behaviour (no line color) for existing callers/tests. This mirrors the GLSL
// compositor's normal-path block 1:1.
inline uint32_t compositeNormal(const std::array<uint32_t, kLayerCount>& color,
                                const std::array<uint32_t, kLayerCount>& attr,
                                uint32_t backColor,
                                BlendMode mode,
                                bool ratioFromSecond,
                                int lineScreenMask = 0,
                                uint32_t lineColor = 0,
                                uint8_t lineColorAlpha = 0) {
    // Select top + second using the same loop as compositeBasic.
    int topIdx = -1;
    int secondIdx = -1;
    for (int priority = kMaxPriority; priority > 0 && secondIdx < 0; --priority) {
        for (int which = kSprite; which >= 0 && secondIdx < 0; --which) {
            Attr a = unpackAttr(attr[which]);
            if (a.transparent) continue;
            if (clampPriority(a.priority) == 0) continue;
            if (clampPriority(a.priority) != priority) continue;
            if (topIdx < 0) {
                topIdx = which;
            } else {
                secondIdx = which;
            }
        }
    }

    if (topIdx < 0) {
        return backColor & 0x00FFFFFFu;  // all transparent -> back screen
    }

    Attr topAttr = unpackAttr(attr[topIdx]);
    uint32_t topColor = color[topIdx] & 0x00FFFFFFu;

    // Second operand: another opaque layer, or the back screen. The back screen
    // has no color calc (ratio 0x3F, ccEnable false).
    uint32_t secondColor;
    Attr secondAttr;
    if (secondIdx >= 0) {
        secondColor = color[secondIdx] & 0x00FFFFFFu;
        secondAttr = unpackAttr(attr[secondIdx]);
    } else {
        secondColor = backColor & 0x00FFFFFFu;
        secondAttr.ccEnable = false;
        secondAttr.ccRatio = 0x3F;
    }

    // Build titan pixels: alpha = ccRatio, bit31 = ccEnable (trans flag proxy).
    uint32_t topPixel = createPixel(static_cast<uint8_t>(topAttr.ccRatio & 0x3F),
                                    getRed(topColor), getGreen(topColor),
                                    getBlue(topColor));
    if (topAttr.ccEnable) topPixel |= 0x80000000u;

    uint32_t secondPixel = createPixel(
        static_cast<uint8_t>(secondAttr.ccRatio & 0x3F),
        getRed(secondColor), getGreen(secondColor), getBlue(secondColor));
    if (secondAttr.ccEnable) secondPixel |= 0x80000000u;

    // Normal-path LNCL line color screen. the SW reference inserts the line color only
    // inside the color-calc block (pix & PIX_CCE_SHIFT), so this is gated on the
    // top layer's cc-enable. When inserted, the top blends with the line color
    // using the global mode BEFORE the top<->second stage; the result is opaque
    // (bit31 clear), so the gate below skips the (already spent) second blend --
    // the line color has taken the second slot.
    if (topAttr.ccEnable && ((lineScreenMask >> topIdx) & 1)) {
        uint32_t linePixel = createPixel(
            static_cast<uint8_t>(lineColorAlpha & 0x3F),
            getRed(lineColor), getGreen(lineColor), getBlue(lineColor));
        topPixel = applyBlend(mode, topPixel, linePixel);
    }

    // Gate: does color calc run on the top pixel? the SW reference T_MixIt blends iff the
    // TOP layer's color-calc-enable bit is set (pix & PIX_CCE_SHIFT), independent
    // of CCMD/CCRTMD. In the ratio+Top case this matches the old `alpha < 0x3F`
    // gate (a cc-disabled layer decodes to ratio 0x3F). After the normal-path LNCL
    // blend above, the top pixel is opaque (bit31 clear), so the line color has
    // spent the blend and this gate skips the second blend.
    bool doBlend = (topPixel & 0x80000000u) != 0;

    if (!doBlend) {
        return topPixel & 0x00FFFFFFu;
    }

    // Resolve the blend mode against CCRTMD: ratio mode picks Top vs Bottom by
    // ratioFromSecond; Add ignores the ratio. (The compositor passes `mode`
    // already resolved, but keep ratioFromSecond explicit for clarity / parity.)
    (void)ratioFromSecond;
    uint32_t blended = applyBlend(mode, topPixel, secondPixel);
    return blended & 0x00FFFFFFu;
}

// ---------------------------------------------------------------------------
// Extended color calculation (issue #22 task T-014, MVP stage 3, UT-008/UT-B06).
//
// AUTHORITATIVE SPEC: Sega VDP2 User's Manual section 12, p12_12 (extended color
// calculation) + p12_14 (CCCTL/EXCCEN/CCRTMD/CCMD). vidsoft/titan do NOT
// implement this chain (their compositor is a 2-layer top+second model), so this
// oracle is derived from the hardware manual, not from vidsoft.
//
// What extended color calc does (CCCTL.EXCCEN, bit 10, normal TV mode only):
//   It builds an "extended second" operand out of the second/third (/fourth)
//   priority screens by ADDING them at a fixed extended ratio, then blends the
//   extended second with the TOP screen using the SAME normal color calc as
//   compositeNormal (CCRNx ratio / CCMD add, CCRTMD source). i.e. extended cc
//   only changes WHAT the second operand is; the top<->second stage is unchanged.
//
// Chain control (p12_14 note, p12_12 figure 12.3):
//   - top <-> second : gated by the TOP screen ccEnable (the normal-cc gate,
//                      already handled by the final compositeNormal-style stage).
//   - second <-> third (whether third is folded into the extended second):
//                      gated by the SECOND screen ccEnable.
//   - third <-> fourth (whether fourth is folded into third):
//                      gated by the THIRD screen ccEnable. The fourth screen
//                      ONLY participates when a line color screen is inserted
//                      (figure 12.3 "with line color inserted"); without LNCL
//                      the extended second reaches at most second+third.
//
// Extended ratio table (table 12.2), second:third:fourth, applied to each
// channel scaled by 1/4 (manual note: ratios apply to each RGB channel value
// multiplied by 1/4). Without line color insertion (T-014 scope; LNCL is T-015):
//   second.ccEnable == 0 -> 4:0:0  (extended second = second, no fold)
//   second.ccEnable == 1 -> 2:2:0  (extended second = (second + third) / 2)
// With line color inserted (T-015): 4:0:0 / 2:2:0 (LNCL as third) / 2:1:1.
// CRAM mode 0 vs mode1/2 only changes the table when LNCL/RGB-format combine;
// for the no-LNCL palette-format MVP both reduce to the 4:0:0 / 2:2:0 pair.
//
// `lineColorInserted` selects the table branch. For the T-014 MVP it is false;
// the fourth-stage branch is present so T-015 can drive it with the LNCL operand.
// ---------------------------------------------------------------------------

// Per-channel (a + b) / 2 and a / 2 helpers (the SW reference folds images this way).
inline uint32_t halfSum(uint32_t a, uint32_t b) {
    return createPixel(0,
        static_cast<uint8_t>((getRed(a)   + getRed(b))   / 2),
        static_cast<uint8_t>((getGreen(a) + getGreen(b)) / 2),
        static_cast<uint8_t>((getBlue(a)  + getBlue(b))  / 2));
}
inline uint32_t halfOne(uint32_t a) {
    return createPixel(0,
        static_cast<uint8_t>(getRed(a)   >> 1),
        static_cast<uint8_t>(getGreen(a) >> 1),
        static_cast<uint8_t>(getBlue(a)  >> 1));
}

// Build the "extended second" operand. C++ mirror of the production GLSL
// Vdp2Compositor.cpp buildExtendedSecond() (a faithful the SW reference T_MixIt port).
//
//   Line color inserted:
//     CRAM mode 0:   ext2 = (lineColor + (secondCc ? second/2 : second)) / 2
//     CRAM mode 1/2: second RGB  -> ext2 = (lineColor + ((secondCc && thirdRGB)
//                                          ? (second+third)/2 : second)) / 2
//                    second pal  -> ext2 = lineColor   (priority-second excluded)
//   No line color:
//     secondCc && (CRAM0 || thirdRGB) -> ext2 = (second + third) / 2
//     otherwise                       -> ext2 = second
//
// second/third/lineColor are 0x00RRGGBB; a missing layer is black. cram0 ==
// (CRAM mode 0); secondRGB/thirdRGB are the per-layer direct-color (non-palette)
// flags (a missing layer is palette, i.e. cannot be an RGB operand).
inline uint32_t buildExtendedSecond(uint32_t second, uint32_t third,
                                    bool secondCc, bool secondRGB, bool thirdRGB,
                                    bool lineInsert, uint32_t lineColor, bool cram0) {
    if (lineInsert) {
        if (cram0) {
            uint32_t t = secondCc ? halfOne(second) : second;
            return halfSum(lineColor, t);
        }
        if (secondRGB) {
            uint32_t t = (secondCc && thirdRGB) ? halfSum(second, third) : second;
            return halfSum(lineColor, t);
        }
        return lineColor & 0x00FFFFFFu;        // palette second excluded
    }
    // No line color inserted. Table 12.2 (Sega VDP2 manual, the primary source
    // for extended color calc -- vidsoft / the SW reference do not bit-reproduce
    // it): the third image is folded (2:2:0) only when the second image's color
    // operation enable bit is set AND, in CRAM mode 1/2, BOTH the second and the
    // third image are direct RGB. A palette-format second OR third forces 4:0:0
    // (extended second = second). CRAM mode 0 ignores the formats (fold iff
    // secondCc).
    if (secondCc && (cram0 || (secondRGB && thirdRGB))) {
        return halfSum(second, third);
    }
    return second & 0x00FFFFFFu;
}

// Final top<->extended-second blend stage. This is the exact body of
// compositeNormal's blend (titan transTest gate + applyBlend), factored so the
// extended path reuses identical math. `topColor`/`secondColor` are 0x00RRGGBB;
// `topRatio`/`secondRatio`/`topCcEnable`/`secondCcEnable` are the per-operand cc
// attributes (ccRatio 0..0x3F, ccEnable bit). Returns 0x00RRGGBB.
inline uint32_t blendTopSecondStage(uint32_t topColor, uint8_t topRatio, bool topCcEnable,
                                    uint32_t secondColor, uint8_t secondRatio, bool secondCcEnable,
                                    BlendMode mode) {
    uint32_t topPixel = createPixel(static_cast<uint8_t>(topRatio & 0x3F),
                                    getRed(topColor), getGreen(topColor), getBlue(topColor));
    if (topCcEnable) topPixel |= 0x80000000u;
    uint32_t secondPixel = createPixel(static_cast<uint8_t>(secondRatio & 0x3F),
                                       getRed(secondColor), getGreen(secondColor), getBlue(secondColor));
    if (secondCcEnable) secondPixel |= 0x80000000u;

    // the SW reference T_MixIt: blend iff the TOP layer's cc-enable bit is set (matches
    // the ratio+Top `alpha < 0x3F` gate; see compositeNormal).
    bool doBlend = (topPixel & 0x80000000u) != 0;
    if (!doBlend) {
        return topPixel & 0x00FFFFFFu;
    }
    return applyBlend(mode, topPixel, secondPixel) & 0x00FFFFFFu;
}

// Full extended color calc composite. C++ mirror of the production GLSL
// Vdp2Compositor.cpp main() extended path (a faithful the SW reference T_MixIt port).
// Selects top..third by priority (same descending-priority / LayerId-tiebreak
// loop as compositeBasic / compositeNormal), builds the extended-second operand
// via buildExtendedSecond, then runs the top<->extended-second blend. When EXCCEN
// is disabled the caller uses compositeNormal instead; this function assumes
// EXCCEN is on. Returns 0x00RRGGBB.
//
// Missing operands (fewer than three opaque layers) fall back to the back screen
// for the second slot and to BLACK (0) for the third slot.
//
// `paletteMask` bit[layerIdx] == 1 marks that layer as palette format (a missing
// layer is treated as palette). `lineColorInserted` is already gated by the
// caller on (LCCCEN && LNCLEN && EXCCEN) + the top layer carrying the line color
// screen. `lineColorAlpha` is the line color's color-calc ratio (CCRLB-derived):
// when the line color is inserted it becomes the second operand, so under
// CCRTMD=Second (BlendMode::Bottom) the blend ratio comes from it, not from the
// priority-second (the SW reference: pix2 becomes the line color).
inline uint32_t compositeExtended(const std::array<uint32_t, kLayerCount>& color,
                                  const std::array<uint32_t, kLayerCount>& attr,
                                  uint32_t backColor,
                                  BlendMode mode,
                                  bool ratioFromSecond,
                                  bool lineColorInserted,
                                  uint32_t lineColor = 0u,
                                  int cramMode = 0,
                                  uint32_t paletteMask = 0u,
                                  uint8_t lineColorAlpha = 0x3F) {
    (void)ratioFromSecond;  // mode already resolved by the caller (CCRTMD).
    // Select up to four opaque layers in priority order.
    int idx[4] = {-1, -1, -1, -1};
    int found = 0;
    for (int priority = kMaxPriority; priority > 0 && found < 4; --priority) {
        for (int which = kSprite; which >= 0 && found < 4; --which) {
            Attr a = unpackAttr(attr[which]);
            if (a.transparent) continue;
            if (clampPriority(a.priority) == 0) continue;
            if (clampPriority(a.priority) != priority) continue;
            idx[found++] = which;
        }
    }

    if (idx[0] < 0) {
        return backColor & 0x00FFFFFFu;  // all transparent -> back screen
    }

    Attr topAttr = unpackAttr(attr[idx[0]]);
    uint32_t topColor = color[idx[0]] & 0x00FFFFFFu;

    // Second operand: opaque layer or back screen (back has no color calc).
    uint32_t secondColor;
    Attr secondAttr;
    if (idx[1] >= 0) {
        secondColor = color[idx[1]] & 0x00FFFFFFu;
        secondAttr = unpackAttr(attr[idx[1]]);
    } else {
        secondColor = backColor & 0x00FFFFFFu;
        secondAttr.ccEnable = false;
        secondAttr.ccRatio = 0x3F;
    }

    // Third operand: opaque layer or black.
    uint32_t thirdColor = (idx[2] >= 0) ? (color[idx[2]] & 0x00FFFFFFu) : 0u;

    // Per-layer direct-color (non-palette) flags; a missing layer is palette.
    bool secondRGB = (idx[1] >= 0) && (((paletteMask >> idx[1]) & 1u) == 0u);
    bool thirdRGB  = (idx[2] >= 0) && (((paletteMask >> idx[2]) & 1u) == 0u);

    uint32_t extSecond = buildExtendedSecond(
        secondColor, thirdColor, secondAttr.ccEnable, secondRGB, thirdRGB,
        lineColorInserted, lineColor & 0x00FFFFFFu, cramMode == 0);

    // Under CCRTMD=Second the line color (now pix2) supplies the ratio.
    uint8_t secondRatio = lineColorInserted
                              ? static_cast<uint8_t>(lineColorAlpha & 0x3F)
                              : static_cast<uint8_t>(secondAttr.ccRatio);

    return blendTopSecondStage(topColor, topAttr.ccRatio, topAttr.ccEnable,
                               extSecond, secondRatio, secondAttr.ccEnable,
                               mode);
}

}  // namespace vdp2oracle

#endif  // VDP2_COLOR_ORACLE_H
