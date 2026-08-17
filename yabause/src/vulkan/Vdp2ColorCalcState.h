// Copyright 2026 devMiyax
//
// VDP2 color-calculation register decoder (issue #22, task T-005).
//
// Decodes the global VDP2 color-calculation registers (CCCTL / CCRNA / CCRNB /
// CCRR / CCRS{A..D} / CCRLB / CCRTMD) into per-layer color-calc state that the
// new per-pixel compositor (Vdp2Compositor, T-008) and its GLSL shader consume.
//
// This header is the SINGLE SOURCE OF TRUTH for the decode math. Both sides use
// it:
//   - host-port unit tests (vulkan/test/vdp2_compositor_test.cpp, UT-001/002/009)
//     include it directly and compare against vdp2_color_oracle.h.
//   - the Vulkan port (VIDVulkan::readVdp2ColorCalcState(), next to
//     readVdp2ColorOffset()) extracts the raw register values from the Vdp2
//     register struct and calls decodeVdp2ColorCalc() below.
//
// The pure decoder takes raw u16 register values (NOT the emulator Vdp2 struct)
// so the standalone gtest target can link it without the emulator core. The GLSL
// compositor (T-008) must reproduce the same per-layer ratio / mode / source
// decode; keep the bit math here as the reference.
//
// SOURCE OF TRUTH for the per-layer ratio decode is vidsoft.c (the bit-accurate
// software renderer), cross-checked against vdp2_color_oracle.h decodeLayerAlpha:
//   vidsoft.c:1639  NBG0: if (CCCTL & 0x201) alpha = ((~CCRNA      & 0x1F) << 1)+1
//   vidsoft.c:1744  NBG1: if (CCCTL & 0x202) alpha = ((~CCRNA & 0x1F00) >> 7)+1
//   vidsoft.c:1831  NBG2: if (CCCTL & 0x204) alpha = ((~CCRNB      & 0x1F) << 1)+1
//   vidsoft.c:1904  NBG3: if (CCCTL & 0x208) alpha = ((~CCRNB & 0x1F00) >> 7)+1
//   vidsoft.c:2036  RBG0: if (CCCTL & 0x210) alpha = ((~CCRR       & 0x1F) << 1)+1
//
// CCCTL bit layout (Sega VDP2 User's Manual hard/vdp2/hon/p12_14.htm, the
// authoritative source; the per-screen color-calc enable nibble is wider than 4
// bits -- it covers bits 0..6):
//   bit 0 0x01: N0CCEN (NBG0)
//   bit 1 0x02: N1CCEN (NBG1)
//   bit 2 0x04: N2CCEN (NBG2)
//   bit 3 0x08: N3CCEN (NBG3)
//   bit 4 0x10: R0CCEN (RBG0)
//   bit 5 0x20: LCCCEN (line color screen / LNCL)
//   bit 6 0x40: SPCCEN (sprite)
//   bit 8 0x100: CCMD  -> color-calc mode (0 = ratio, 1 = additive)
//   bit 9 0x200: CCRTMD -> color-calc ratio mode / ratio source select
//                0 = source (top)   alpha -> titan TITAN_BLEND_TOP
//                1 = destination (second) alpha -> titan TITAN_BLEND_BOTTOM
//
// CCRTMD note (vidsoft has NO standalone branch for it; see task brief): vidsoft
// folds CCRTMD into the titan blend-mode selection in VIDSoftVdp2DrawStart
// (vidsoft.c:3519-3522): CCMD(0x100) -> TITAN_BLEND_ADD; else CCRTMD(0x200) ->
// TITAN_BLEND_BOTTOM; else TITAN_BLEND_TOP. TITAN_BLEND_TOP takes the blend
// ratio from the TOP pixel (oracle blendTop), TITAN_BLEND_BOTTOM from the SECOND
// pixel (oracle blendBottom). So CCRTMD is exactly "which operand supplies the
// ratio". We surface it here as RatioSource so the compositor can pick the same
// operand. The per-layer ratio VALUE is identical regardless of CCRTMD (vidsoft
// stores the same info.alpha); CCRTMD only changes which layer's stored ratio is
// used at blend time.
//
// ASCII-only comments (CLAUDE.md rule: MSVC CP932 -> C4819/C2065).

#ifndef VDP2_COLOR_CALC_STATE_H
#define VDP2_COLOR_CALC_STATE_H

#include <cstdint>
#include <array>

namespace vdp2cc {

// ---------------------------------------------------------------------------
// CCCTL bit masks. Defined once here and reused by host-port + (mirrored in)
// GLSL so the two never drift.
// ---------------------------------------------------------------------------
enum CcctlBits : uint16_t {
    kCcEnNBG0  = 0x0001,
    kCcEnNBG1  = 0x0002,
    kCcEnNBG2  = 0x0004,
    kCcEnNBG3  = 0x0008,
    kCcEnRBG0  = 0x0010,
    kCcEnLNCL  = 0x0020,   // LCCCEN: line color screen color-calc enable (bit 5)
    kCcEnSprite = 0x0040,  // SPCCEN: sprite color-calc enable (bit 6)
    kCcmd      = 0x0100,   // 0 = ratio, 1 = additive
    kCcrtmd    = 0x0200,   // 0 = top alpha source, 1 = second alpha source
    kExccen    = 0x0400,   // EXCCEN: extended color calc enable (CCCTL bit 10)
};

// Global color-calc mode (CCMD), shared by all layers.
enum class CalcMode : uint8_t {
    Ratio = 0,  // CCMD = 0: ratio blend (titan TOP/BOTTOM depending on CCRTMD)
    Add   = 1,  // CCMD = 1: additive blend (titan TITAN_BLEND_ADD)
};

// CCRTMD: which operand supplies the color-calc ratio.
enum class RatioSource : uint8_t {
    Top    = 0,  // CCRTMD = 0: ratio from the top layer (TITAN_BLEND_TOP)
    Second = 1,  // CCRTMD = 1: ratio from the second layer (TITAN_BLEND_BOTTOM)
};

// Logical layer ids. Match vdp2_color_oracle.h LayerId ordering so the host
// arrays line up 1:1 (NBG3..NBG0, RBG0, Sprite). Index into perLayer[].
enum LayerIndex {
    kNBG3 = 0,
    kNBG2 = 1,
    kNBG1 = 2,
    kNBG0 = 3,
    kRBG0 = 4,
    kSprite = 5,
    kLayerCount = 6,
};

// Per-layer decoded color-calc state.
struct LayerCalc {
    bool ccEnable = false;   // CCCTL per-screen enable bit set for this layer.
                             // This is the BLEND GATE: whether color calc runs
                             // when this layer is the top image. It is the
                             // enable bit ONLY -- see decodeLayer() for why
                             // CCRTMD must not be OR'd in here.
    uint8_t ratio = 0x3F;    // color-calc ratio (0..0x3F). 0x3F when cc disabled,
                             // matching vidsoft's "else info.alpha = 0x3F".
    bool transFlag = false;  // the 0x80 ("trans" / cc-on) flag vidsoft ORs into
                             // info.alpha; drives ADD/BOTTOM blend selection.
};

// Full decoded state passed to the compositor UBO.
struct State {
    CalcMode mode = CalcMode::Ratio;          // CCMD
    RatioSource ratioSource = RatioSource::Top; // CCRTMD
    bool extendedEnable = false;              // EXCCEN: extended color calc on
    std::array<LayerCalc, kLayerCount> perLayer{};
    // Sprite color-calc ratio table (CCRTMD-independent), CCRS{A..D} decoded.
    // Mirrors vidsoft.c colorcalctable[8] / oracle decodeSpriteColorCalcTable.
    std::array<uint8_t, 8> spriteRatioTable{};
    // Line color screen ratio (CCRLB low 5 bits, NO +1; see oracle
    // decodeLineColorAlpha and the T-001 hand-off note).
    uint8_t lineColorRatio = 0;
};

// ---------------------------------------------------------------------------
// Per-layer ratio decode. Mirrors vidsoft.c and vdp2_color_oracle.h
// decodeLayerAlpha exactly. `ratioField` 0 = low 5 bits, 1 = high 5 bits
// (bits 8..12). The `extBit` is CCRTMD (0x200); the trans flag follows vidsoft:
//   if ((CCCTL & (CCRTMD|enable)) == (CCRTMD|enable)) trans
//   else if ((CCCTL & (CCMD|enable)) == (CCMD|enable)) trans
// ---------------------------------------------------------------------------
inline LayerCalc decodeLayer(uint16_t cctl,
                             uint16_t ratioReg,
                             int ratioField,
                             uint16_t enableBit) {
    LayerCalc out;
    const uint16_t extBit = kCcrtmd;  // 0x200
    // RATIO value: vidsoft computes info.alpha under `CCCTL & (0x200|enable)`.
    // The 0x200 (CCRTMD) half of that OR exists so a layer can SUPPLY its ratio
    // when it is the SECOND operand in ratio-from-second mode, even with its own
    // enable bit clear. It is NOT a blend gate.
    if (cctl & (extBit | enableBit)) {
        uint16_t ratio5;
        if (ratioField == 0) {
            ratio5 = static_cast<uint16_t>((~ratioReg) & 0x1F);
        } else {
            // ((~reg & 0x1F00) >> 8) == ((~reg & 0x1F00) >> 7) >> 1 ; keep the
            // 5-bit value then (<<1)+1 below to match the oracle exactly.
            ratio5 = static_cast<uint16_t>(((~ratioReg) & 0x1F00) >> 8);
        }
        out.ratio = static_cast<uint8_t>((ratio5 << 1) + 1);
    } else {
        out.ratio = 0x3F;
    }
    // BLEND GATE (ccEnable): whether color calc runs when this layer is the TOP
    // image. In titan this is transTest(): TOP mode gates on alpha < 0x3F (alpha
    // decoded only when the enable bit is set, since CCRTMD == 0 there), and
    // BOTTOM / ADD modes gate on the 0x80 trans flag (enable AND CCRTMD/CCMD).
    // In every mode the gate reduces to the layer's own enable bit -- OR'ing
    // CCRTMD in here (the old behaviour) made ratio-from-second games blend
    // EVERY layer half-transparent (Space Harrier NBG1/NBG2 wash-out).
    out.ccEnable = (cctl & enableBit) != 0;
    // 0x80 trans flag (vidsoft info.alpha |= 0x80).
    if ((cctl & (extBit | enableBit)) == (extBit | enableBit)) {
        out.transFlag = true;
    } else if ((cctl & (kCcmd | enableBit)) == (kCcmd | enableBit)) {
        out.transFlag = true;
    }
    return out;
}

// Sprite color-calc ratio table. Port of vidsoft.c colorcalctable[8] /
// oracle decodeSpriteColorCalcTable. `ccrs` = {CCRSA, CCRSB, CCRSC, CCRSD}.
inline std::array<uint8_t, 8> decodeSpriteTable(const std::array<uint16_t, 4>& ccrs) {
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

// Line color screen ratio. Port of vidsoft.c:1516 (CCRLB & 0x1F) << 1, NO +1.
inline uint8_t decodeLineColorRatio(uint16_t ccrlb) {
    return static_cast<uint8_t>((ccrlb & 0x1F) << 1);
}

// ---------------------------------------------------------------------------
// Top-level decode. Raw register values in; full State out. The Vulkan port
// (VIDVulkan::readVdp2ColorCalcState) fills the args from the Vdp2 reg struct
// and calls this; the GLSL compositor reproduces the same per-layer result.
// ---------------------------------------------------------------------------
inline State decodeVdp2ColorCalc(uint16_t cctl,
                                 uint16_t ccrna,
                                 uint16_t ccrnb,
                                 uint16_t ccrr,
                                 const std::array<uint16_t, 4>& ccrs,
                                 uint16_t ccrlb) {
    State s;
    s.mode = (cctl & kCcmd) ? CalcMode::Add : CalcMode::Ratio;
    s.ratioSource = (cctl & kCcrtmd) ? RatioSource::Second : RatioSource::Top;
    s.extendedEnable = (cctl & kExccen) != 0;

    s.perLayer[kNBG0] = decodeLayer(cctl, ccrna, /*field*/ 0, kCcEnNBG0);
    s.perLayer[kNBG1] = decodeLayer(cctl, ccrna, /*field*/ 1, kCcEnNBG1);
    s.perLayer[kNBG2] = decodeLayer(cctl, ccrnb, /*field*/ 0, kCcEnNBG2);
    s.perLayer[kNBG3] = decodeLayer(cctl, ccrnb, /*field*/ 1, kCcEnNBG3);
    s.perLayer[kRBG0] = decodeLayer(cctl, ccrr, /*field*/ 0, kCcEnRBG0);

    // Sprite per-layer ccEnable / trans uses the sprite enable bit (0x20). Its
    // ratio is selected per-pixel from spriteRatioTable by the sprite's
    // color-calc code (SPCTL), so the per-layer single ratio is left at table[0]
    // as a sensible default; the compositor indexes the table for the actual
    // sprite pixel.
    {
        LayerCalc spr;
        // Same enable-bit-only gate as decodeLayer (see comment there).
        spr.ccEnable = (cctl & kCcEnSprite) != 0;
        if ((cctl & (kCcrtmd | kCcEnSprite)) == (kCcrtmd | kCcEnSprite)) {
            spr.transFlag = true;
        } else if ((cctl & (kCcmd | kCcEnSprite)) == (kCcmd | kCcEnSprite)) {
            spr.transFlag = true;
        }
        s.spriteRatioTable = decodeSpriteTable(ccrs);
        spr.ratio = s.spriteRatioTable[0];
        s.perLayer[kSprite] = spr;
    }

    s.lineColorRatio = decodeLineColorRatio(ccrlb);
    return s;
}

}  // namespace vdp2cc

#endif  // VDP2_COLOR_CALC_STATE_H
