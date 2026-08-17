// Copyright 2026 devMiyax
//
// VDP2 sprite decode reference (issue #22, task T-009).
//
// Pure, side-effect-free host port of the sprite-slice population logic for the
// new VDP2 G-buffer compositor. Given a VDP1 framebuffer pixel (already decoded
// by the VDP1 rasterizer into the renderer's RGBA framebuffer encoding) plus the
// VDP2 sprite registers, it produces the (transparent / priority / ccEnable /
// ccRatio / color) tuple the compositor stores in the kSprite slice.
//
// SOURCE OF TRUTH (do not diverge without updating both sides):
//   - VDP1 framebuffer alpha-byte encoding ... yglshaderes.c
//       Yglprg_vdp2_drawfb_cram_vulkan_f + vdp1_compute_tile_shade.comp
//       encodeAlphaByte(): bit7 show, bit6 C(palette), bits5:3 colorcl,
//       bits2:0 priority-slot.
//   - sprite color-calc transparency ......... vidsoft.c VIDSoftVdp2DrawSprite
//       (SPCCCS cases 0..3; CCRTMD/CCMD trans-flag handling).
//   - sprite color-calc ratio table .......... vidsoft.c colorcalctable[8]
//       (== vdp2_color_oracle.h decodeSpriteColorCalcTable).
//
// IMPORTANT (architecture): the sprite-type-dependent bit slicing
// (Vdp1GetSpritePixelInfo in vidshared.h: type 0..F -> priority bits / colorcalc
// bits / shadow) is performed UPSTREAM by the VDP1 rasterizer when it writes the
// framebuffer. By the time the compositor reads the framebuffer, the alpha byte
// already carries the resolved priority SLOT (0..7, an index into the PRISA..D
// priority table) and the color-calc CODE (0..7, an index into the sprite
// color-calc ratio table). decodeSprite() therefore consumes the framebuffer
// encoding, exactly like the existing FramebufferRenderer GLSL shader does, and
// does NOT re-run the sprite-type bit slicing. This matches the design note in
// 01-design.md section 2.5 (reuse/port the FramebufferRenderer logic).
//
// Shadow (normal + MSB) is phase2 (01-design.md section 2.6): decodeSprite reads
// the shadow flag for completeness but the compositor does not apply the shadow
// blend effect in the MVP. UT-010 only pins priority / cc / color.
//
// The GLSL counterpart (Vdp2SpriteDecoder fragment shader) MUST reproduce the
// same per-pixel decode. ASCII-only comments (CLAUDE.md rule: MSVC CP932 ->
// C4819/C2065).

#ifndef VDP2_SPRITE_DECODE_H
#define VDP2_SPRITE_DECODE_H

#include <array>
#include <cstdint>

#include "vdp2_color_oracle.h"

namespace vdp2sprite {

// VDP1 framebuffer pixel as the Vulkan renderer stores it: RGBA8, where the
// alpha byte is the "additional" word (see encodeAlphaByte). For an index-color
// (palette) sprite the R/G bytes carry the low/high palette index byte and B
// carries the shadow flag; for a direct-color sprite R/G/B is the color.
struct FbPixel {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;  // additional byte
};

// Decoded sprite contribution for one pixel (what the kSprite slice stores).
struct Decoded {
    bool transparent = true;   // true -> slice stays at the clear value
    uint8_t priority = 0;      // resolved VDP2 priority value 0..7 (post table)
    bool ccEnable = false;     // color-calc enabled for this sprite pixel
    uint8_t ccRatio = 0x3F;    // color-calc ratio 0..0x3F
    uint32_t color = 0;        // 0x00RRGGBB (color offset NOT applied here)
    bool shadow = false;       // normal/MSB shadow flag (phase2; informational)
};

// Register inputs needed for the sprite decode. priorityTable[slot] is the
// resolved VDP2 priority for slot 0..7 (PRISA..D & 0x7, the same table vidsoft
// builds). spriteRatioTable[code] is the decoded color-calc ratio for code 0..7
// (== Vdp2ColorCalcState spriteRatioTable / oracle decodeSpriteColorCalcTable).
struct SpriteRegs {
    std::array<uint8_t, 8> priorityTable{};   // PRISA..D nibbles, & 0x7
    std::array<uint8_t, 8> spriteRatioTable{};
    uint16_t spctl = 0;   // SPCTL (SPCCCS at bits 12-13, SPCCN at bits 8-10)
    uint16_t ccctl = 0;   // CCCTL (sprite cc enable 0x40 etc.)
    uint32_t colorMode = 0;       // Vdp2Internal.ColorMode (0/1/2) for CRAM fetch
    uint32_t colorRamOffset = 0;  // (CRAOFB & 0x70) << 4
    bool spriteWindow = false;    // SPCTL sprite-window enable (affects 0-index)
};

// Alpha-byte field accessors (mirror encodeAlphaByte / the drawfb GLSL).
inline bool fbShow(uint8_t additional)      { return (additional & 0x80) != 0; }
inline bool fbIsPalette(uint8_t additional) { return (additional & 0x40) != 0; }
inline uint8_t fbColorCalc(uint8_t additional) { return (additional >> 3) & 0x07; }
inline uint8_t fbPrioritySlot(uint8_t additional) { return additional & 0x07; }

// Decode a single VDP1 framebuffer pixel into its kSprite-slice contribution.
//
// `cram` may be null when no index-color pixel is expected; index-color decode
// fetches from it via the oracle cramGetColor() (the same CRAM image the GLSL
// path texelFetches). The flow mirrors Yglprg_vdp2_drawfb_cram_vulkan_f:
//   1. show bit clear -> transparent.
//   2. palette pixel: shadow (b >= 0.45*255 ~= 0x73) handled as shadow flag;
//      colindex 0 with sprite-window / slot 0 -> transparent (drawfb rule);
//      else fetch CRAM color.
//   3. direct pixel: use R/G/B verbatim (sprite-window all-zero -> transparent).
//   4. priority = priorityTable[slot]; a resolved priority of 0 means the layer
//      is not drawn (titan skips priority 0) -> transparent for sorting.
//   5. color calc: SPCCCS transparency test + CCRTMD/CCMD trans flag, ccRatio
//      from spriteRatioTable[colorcalc code].
inline Decoded decodeSprite(const FbPixel& px, const SpriteRegs& regs,
                            const uint8_t* cram) {
    Decoded out;

    if (!fbShow(px.a)) {
        return out;  // transparent (show bit clear)
    }

    const uint8_t slot = fbPrioritySlot(px.a);
    const uint8_t colorCalcCode = fbColorCalc(px.a);

    uint32_t color = 0;
    bool transparentPixel = false;

    if (fbIsPalette(px.a)) {
        // Index-color path. b >= 0.45 (~0x73) marks a shadow pixel in the
        // renderer's encoding; phase2 only flags it.
        if (px.b >= 0x73) {
            out.shadow = true;
        }
        uint32_t colindex =
            (static_cast<uint32_t>(px.g) << 8) | static_cast<uint32_t>(px.r);
        if (colindex == 0) {
            // hard/vdp1/hon/p02_11.htm: index 0 is ignored when sprite window
            // is enabled OR the priority slot is 0.
            if (regs.spriteWindow || slot == 0) {
                transparentPixel = true;
            }
        }
        colindex += regs.colorRamOffset;
        if (cram != nullptr) {
            color = vdp2oracle::cramGetColor(regs.colorMode, colindex, cram)
                    & 0x00FFFFFFu;
        }
    } else {
        // Direct-color path.
        if (regs.spriteWindow && px.r == 0 && px.g == 0 && px.b == 0) {
            transparentPixel = true;
        }
        color = (static_cast<uint32_t>(px.r) << 16)
              | (static_cast<uint32_t>(px.g) << 8)
              | static_cast<uint32_t>(px.b);
    }

    const uint8_t priority = (slot < 8) ? (regs.priorityTable[slot] & 0x7) : 0;

    // Priority 0 -> not drawn (titan skips priority 0); treat as transparent so
    // the compositor's selectSortedLayers excludes it (same as oracle).
    if (transparentPixel || priority == 0) {
        out.transparent = true;
        return out;
    }

    out.transparent = false;
    out.priority = priority;
    out.color = color;

    // Sprite color calculation. Enabled when CCCTL sprite-cc window bit (0x40)
    // is set (the per-screen sprite enable / cc-on). Ratio from the sprite
    // color-calc table indexed by the color-calc code; transparency test by
    // SPCCCS selects whether cc actually applies for this pixel.
    const int SPCCCS = (regs.spctl >> 12) & 0x3;
    const int SPCCN = (regs.spctl >> 8) & 0x7;
    const bool ccWindowOn = (regs.ccctl & 0x40) != 0;

    if (ccWindowOn) {
        bool ccApplies = false;
        switch (SPCCCS) {
            case 0: ccApplies = (priority <= SPCCN); break;  // less or equal
            case 1: ccApplies = (priority == SPCCN); break;  // equal
            case 2: ccApplies = (priority >= SPCCN); break;  // greater or equal
            case 3: ccApplies = out.shadow; break;            // MSB (approx)
            default: break;
        }
        if (ccApplies) {
            out.ccEnable = true;
            const uint8_t code = colorCalcCode & 0x7;
            out.ccRatio = regs.spriteRatioTable[code];
        }
    }

    return out;
}

// Pack a decoded sprite into the G-buffer attr word (same layout as
// vdp2_color_oracle.h packAttr). transparent slots are left at the clear value
// (0 with the transparent bit set) by the caller; this packs an OPAQUE sprite.
inline uint32_t packSpriteAttr(const Decoded& d) {
    vdp2oracle::Attr a;
    a.priority = d.priority;
    a.ccEnable = d.ccEnable;
    a.ccRatio = d.ccRatio;
    a.transparent = d.transparent;
    return vdp2oracle::packAttr(a);
}

}  // namespace vdp2sprite

#endif  // VDP2_SPRITE_DECODE_H
