// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// Vdp2Snapshot implementation. See Vdp2Snapshot.h for design notes.
//
// NOTE: ASCII-only. No Vulkan / no globals; safe to link into the unit
// test executable without the emulation core.
#include "Vdp2Snapshot.h"

#include <chrono>
#include <cstring>

Vdp2Snapshot Vdp2Snapshot::takeRaw(const Vdp2&                          regs,
                                    const Vdp2&                          fixRegs,
                                    const uint8_t*                       vdp2Ram,
                                    size_t                               vdp2RamSize,
                                    const uint8_t*                       cram,
                                    size_t                               cramSize,
                                    const vdp2Lineinfo*                  lineNBG0Src,
                                    const vdp2Lineinfo*                  lineNBG1Src,
                                    const vdp2rotationparameter_struct&  paraA,
                                    const vdp2rotationparameter_struct&  paraB,
                                    int                                  screenWidth,
                                    int                                  screenHeight,
                                    uint64_t                             frameId)
{
    Vdp2Snapshot s;
    s.frameId      = frameId;
    s.screenWidth  = screenWidth;
    s.screenHeight = screenHeight;
    auto now = std::chrono::system_clock::now().time_since_epoch();
    s.timestampMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count()
    );

    s.regs    = regs;
    s.fixRegs = fixRegs;
    s.paraA   = paraA;
    s.paraB   = paraB;

    if (vdp2Ram != nullptr && vdp2RamSize > 0) {
        s.vdp2Ram.assign(vdp2Ram, vdp2Ram + vdp2RamSize);
    }
    if (cram != nullptr && cramSize > 0) {
        s.cram.assign(cram, cram + cramSize);
    }
    if (lineNBG0Src != nullptr) {
        std::memcpy(s.lineNBG0.data(), lineNBG0Src,
                    sizeof(vdp2Lineinfo) * s.lineNBG0.size());
    }
    if (lineNBG1Src != nullptr) {
        std::memcpy(s.lineNBG1.data(), lineNBG1Src,
                    sizeof(vdp2Lineinfo) * s.lineNBG1.size());
    }
    return s;
}

void Vdp2Snapshot::decodeLayers() {
    // Names + isRotation are static.
    static constexpr const char* kNames[6] = {
        "NBG0", "NBG1", "NBG2", "NBG3", "RBG0", "RBG1"
    };
    for (int i = 0; i < 6; ++i) {
        layers[i].name       = kNames[i];
        layers[i].isRotation = (i == 4 || i == 5);   // RBG0/RBG1
    }

    // -------- BGON: enable bits --------
    //   bit 0 = NBG0, bit 1 = NBG1, bit 2 = NBG2, bit 3 = NBG3,
    //   bit 4 = RBG0, bit 5 = RBG1.
    const uint16_t bgon = regs.BGON;
    layers[0].enabled = (bgon & 0x01) != 0;
    layers[1].enabled = (bgon & 0x02) != 0;
    layers[2].enabled = (bgon & 0x04) != 0;
    layers[3].enabled = (bgon & 0x08) != 0;
    layers[4].enabled = (bgon & 0x10) != 0;
    layers[5].enabled = (bgon & 0x20) != 0;

    // -------- Priority: PRINA (NBG0,NBG1), PRINB (NBG2,NBG3), PRIR (RBG0)
    //   PRINA bits 0-2 = NBG0,  bits 8-10 = NBG1
    //   PRINB bits 0-2 = NBG2,  bits 8-10 = NBG3
    //   PRIR  bits 0-2 = RBG0
    // RBG1 priority follows NBG0's slot when used (CHCTLB selects).
    const uint16_t prina = regs.PRINA;
    const uint16_t prinb = regs.PRINB;
    const uint16_t prir  = regs.PRIR;
    layers[0].priority = (prina >> 0) & 0x07;
    layers[1].priority = (prina >> 8) & 0x07;
    layers[2].priority = (prinb >> 0) & 0x07;
    layers[3].priority = (prinb >> 8) & 0x07;
    layers[4].priority = (prir  >> 0) & 0x07;
    layers[5].priority = (prina >> 0) & 0x07;   // shares NBG0 slot

    // -------- CHCTLA / CHCTLB: color mode + char size + bitmap flag --------
    // CHCTLA layout (NBG0 in low byte, NBG1 in high byte):
    //   bit  0    = NBG0 char size (0=1x1, 1=2x2)
    //   bit  1    = NBG0 bitmap (1=bitmap)
    //   bits 4-6  = NBG0 color number (0=4bpp..4=32bpp)
    //   bit  8    = NBG1 char size
    //   bit  9    = NBG1 bitmap
    //   bits 12-13 = NBG1 color number (only 2 bits in spec)
    const uint16_t chctla = regs.CHCTLA;
    layers[0].charSize   = ((chctla >> 0) & 0x01) ? 2 : 1;
    layers[0].isBitmap   = ((chctla >> 1) & 0x01) != 0;
    layers[0].colorMode  = (chctla >> 4) & 0x07;
    layers[1].charSize   = ((chctla >> 8) & 0x01) ? 2 : 1;
    layers[1].isBitmap   = ((chctla >> 9) & 0x01) != 0;
    layers[1].colorMode  = (chctla >> 12) & 0x03;

    // CHCTLB layout:
    //   bits 0-1   = NBG2 color number
    //   bit  4     = NBG2 char size
    //   bits 5-6   = NBG3 color number
    //   bit  8     = NBG3 char size
    //   bits 12-14 = RBG0 color number
    //   bit  15... = RBG0 char size and bitmap (varies; ignored in MVP)
    const uint16_t chctlb = regs.CHCTLB;
    layers[2].colorMode  = (chctlb >> 0) & 0x03;
    layers[2].charSize   = ((chctlb >> 4) & 0x01) ? 2 : 1;
    layers[2].isBitmap   = false;
    layers[3].colorMode  = (chctlb >> 5) & 0x03;
    layers[3].charSize   = ((chctlb >> 8) & 0x01) ? 2 : 1;
    layers[3].isBitmap   = false;
    layers[4].colorMode  = (chctlb >> 12) & 0x07;
    layers[4].charSize   = 1;
    layers[4].isBitmap   = false;
    layers[5].colorMode  = layers[0].colorMode;   // shares NBG0 slot
    layers[5].charSize   = layers[0].charSize;
    layers[5].isBitmap   = layers[0].isBitmap;

    // -------- Transparency: BGON bits 8-13 (one per layer) --------
    //   bit 8  = NBG0 transparent disable
    //   bit 9  = NBG1 transparent disable
    //   bit 10 = NBG2 transparent disable
    //   bit 11 = NBG3 transparent disable
    //   bit 12 = RBG0 transparent disable
    // "transparent" = NOT disabled. (See VIDVulkan.cpp:2501)
    layers[0].transparent = (bgon & 0x0100) == 0;
    layers[1].transparent = (bgon & 0x0200) == 0;
    layers[2].transparent = (bgon & 0x0400) == 0;
    layers[3].transparent = (bgon & 0x0800) == 0;
    layers[4].transparent = (bgon & 0x1000) == 0;
    layers[5].transparent = layers[0].transparent;   // shares NBG0

    // -------- CCCTL: color calculation enable per layer --------
    //   bit 0 = NBG0, bit 1 = NBG1, bit 2 = NBG2, bit 3 = NBG3, bit 4 = RBG0
    const uint16_t ccctl = regs.CCCTL;
    layers[0].colorCalcEnabled = (ccctl & 0x01) != 0;
    layers[1].colorCalcEnabled = (ccctl & 0x02) != 0;
    layers[2].colorCalcEnabled = (ccctl & 0x04) != 0;
    layers[3].colorCalcEnabled = (ccctl & 0x08) != 0;
    layers[4].colorCalcEnabled = (ccctl & 0x10) != 0;
    layers[5].colorCalcEnabled = layers[0].colorCalcEnabled;   // shares NBG0

    // -------- PLSZ: plane size per layer (2 bits each) --------
    //   0 = 1x1, 1 = 2x1, 2 = 2x2 (3 unused, treated as 2x2)
    //   bits 0-1 = NBG0, 2-3 = NBG1, 4-5 = NBG2, 6-7 = NBG3, 8-9 = RPA, 12-13 = RPB
    const uint16_t plsz = regs.PLSZ;
    layers[0].planeSize = (plsz >> 0) & 0x3;
    layers[1].planeSize = (plsz >> 2) & 0x3;
    layers[2].planeSize = (plsz >> 4) & 0x3;
    layers[3].planeSize = (plsz >> 6) & 0x3;
    layers[4].planeSize = (plsz >> 8) & 0x3;
    layers[5].planeSize = layers[0].planeSize;

    // -------- Scroll registers --------
    // NBG0/1 have integer (SCXINn / SCYINn, 11-bit) and fractional
    // (SCXDNn / SCYDNn). We store the integer part only.
    layers[0].scrollX = regs.SCXIN0 & 0x7FF;
    layers[0].scrollY = regs.SCYIN0 & 0x7FF;
    layers[1].scrollX = regs.SCXIN1 & 0x7FF;
    layers[1].scrollY = regs.SCYIN1 & 0x7FF;
    // NBG2/3 only have integer scroll (SCXNn / SCYNn).
    layers[2].scrollX = regs.SCXN2 & 0x7FF;
    layers[2].scrollY = regs.SCYN2 & 0x7FF;
    layers[3].scrollX = regs.SCXN3 & 0x7FF;
    layers[3].scrollY = regs.SCYN3 & 0x7FF;
    // RBG layers use rotation params; left at 0.

    // -------- Zoom for NBG0/1 --------
    // Mirror VIDVulkan: zoom = 65536 / (reg & 0x7FF00). Guarded against
    // zero to avoid div by zero.
    auto computeZoom = [](uint32_t reg_all) -> float {
        uint32_t denom = reg_all & 0x7FF00;
        return (denom == 0) ? 1.0f : (65536.0f / static_cast<float>(denom));
    };
    layers[0].zoomX = computeZoom(regs.ZMXN0.all);
    layers[0].zoomY = computeZoom(regs.ZMYN0.all);
    layers[1].zoomX = computeZoom(regs.ZMXN1.all);
    layers[1].zoomY = computeZoom(regs.ZMYN1.all);
    // NBG2/3, RBG default to 1.0 (already initialized).

    // -------- MZCTL: mosaic enable per layer (bits 0-4) --------
    const uint16_t mzctl = regs.MZCTL;
    layers[0].mosaicEnabled = (mzctl & 0x01) != 0;
    layers[1].mosaicEnabled = (mzctl & 0x02) != 0;
    layers[2].mosaicEnabled = (mzctl & 0x04) != 0;
    layers[3].mosaicEnabled = (mzctl & 0x08) != 0;
    layers[4].mosaicEnabled = (mzctl & 0x10) != 0;
    layers[5].mosaicEnabled = layers[0].mosaicEnabled;

    // -------- Color calc ratio --------
    //   CCRNA: NBG0 (bits 0-4), NBG1 (bits 8-12)
    //   CCRNB: NBG2 (bits 0-4), NBG3 (bits 8-12)
    //   CCRR:  RBG0 (bits 0-4)
    const uint16_t ccrna = regs.CCRNA;
    const uint16_t ccrnb = regs.CCRNB;
    layers[0].colorCalcRatio = (ccrna >> 0) & 0x1F;
    layers[1].colorCalcRatio = (ccrna >> 8) & 0x1F;
    layers[2].colorCalcRatio = (ccrnb >> 0) & 0x1F;
    layers[3].colorCalcRatio = (ccrnb >> 8) & 0x1F;
    layers[4].colorCalcRatio = (regs.CCRR >> 0) & 0x1F;
    layers[5].colorCalcRatio = layers[0].colorCalcRatio;

    // -------- Color offset --------
    //   CLOFEN: bits 0-4 = enable per layer (NBG0/1/2/3/RBG0)
    //   CLOFSL: same bit layout, selects A (0) or B (1)
    //   COAR/COAG/COAB / COBR/COBG/COBB: 9-bit signed offset (bit 8 sign)
    const uint16_t clofen = regs.CLOFEN;
    const uint16_t clofsl = regs.CLOFSL;
    auto signExtend9 = [](uint16_t v) -> int {
        int x = v & 0x1FF;
        return (x & 0x100) ? (x - 0x200) : x;
    };
    const int coa_r = signExtend9(regs.COAR);
    const int coa_g = signExtend9(regs.COAG);
    const int coa_b = signExtend9(regs.COAB);
    const int cob_r = signExtend9(regs.COBR);
    const int cob_g = signExtend9(regs.COBG);
    const int cob_b = signExtend9(regs.COBB);
    for (int i = 0; i < 5; ++i) {
        const uint16_t bit = 1u << i;
        if ((clofen & bit) == 0) {
            layers[i].colorOffsetSel = -1;
            continue;
        }
        const bool selB = (clofsl & bit) != 0;
        layers[i].colorOffsetSel = selB ? 1 : 0;
        layers[i].colorOffsetR = selB ? cob_r : coa_r;
        layers[i].colorOffsetG = selB ? cob_g : coa_g;
        layers[i].colorOffsetB = selB ? cob_b : coa_b;
    }
    // RBG1 shares NBG0
    layers[5].colorOffsetSel = layers[0].colorOffsetSel;
    layers[5].colorOffsetR   = layers[0].colorOffsetR;
    layers[5].colorOffsetG   = layers[0].colorOffsetG;
    layers[5].colorOffsetB   = layers[0].colorOffsetB;
}
