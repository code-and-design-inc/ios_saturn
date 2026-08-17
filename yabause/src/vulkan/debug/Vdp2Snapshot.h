// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// Vdp2Snapshot: freezes everything needed to reproduce/inspect a single
// VDP2 frame from the Pause UI (F10).
//
// Captures (deep copy):
//   * Vdp2Ram (VDP2 VRAM, typically 512KB)
//   * VDP2 color RAM (4KB)
//   * Vdp2 register block + interlace-adjusted copy
//   * lineNBG0 / lineNBG1 (per-line scroll/coord tables, 512 entries each)
//   * rotation parameter A / B
//
// All fields are owned by the snapshot; the original buffers can mutate
// freely after takeRaw() returns.
//
// decodeLayers() walks regs / fixRegs and fills layers[] with decoded
// human-readable settings. It is pure (no Vulkan / no globals), so it can
// be unit tested without linking the emulation core.
//
// NOTE: ASCII-only file. MSVC default code page on Windows JP builds is
// cp932; non-ASCII chars in source cause C4819 + parser failures.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

extern "C" {
#include "vdp2.h"
#include "vidshared.h"
}

struct Vdp2Snapshot {
    // -------- Meta --------
    uint64_t frameId      = 0;
    uint64_t timestampMs  = 0;
    int      screenWidth  = 0;   // Saturn logical: 320/352/640/704
    int      screenHeight = 0;   // 224/240/256

    // -------- Raw state (deep copies) --------
    std::vector<uint8_t>           vdp2Ram;
    std::vector<uint8_t>           cram;
    Vdp2                           regs{};
    Vdp2                           fixRegs{};
    std::array<vdp2Lineinfo, 512>  lineNBG0{};
    std::array<vdp2Lineinfo, 512>  lineNBG1{};
    vdp2rotationparameter_struct   paraA{};
    vdp2rotationparameter_struct   paraB{};

    // -------- Decoded layer info (populated by decodeLayers()) --------
    struct LayerInfo {
        const char* name             = "";    // "NBG0".."RBG1"
        bool        enabled          = false;
        int         priority         = 0;     // 0..7
        // Color mode encoding (Saturn CHCTLA/B color number):
        //   0 = 4bpp palette
        //   1 = 8bpp palette
        //   2 = 16bpp (RGB 5-5-5)
        //   3 = 16bpp (CRAM)
        //   4 = 32bpp (RGB 8-8-8)
        int         colorMode        = 0;
        // 1 = 1x1 cells (8x8 pix), 2 = 2x2 cells (16x16 pix)
        int         charSize         = 1;
        bool        isBitmap         = false;
        bool        transparent      = true;
        bool        colorCalcEnabled = false;
        bool        isRotation       = false;

        // Plane size encoding (PLSZ 2-bit field per layer):
        //   0 = 1x1, 1 = 2x1, 2 = 2x2 (3 unused, treated as 2x2)
        int         planeSize        = 0;

        // Scroll (integer part of SCXINn / SCYINn for NBG0/1, raw SCXNn /
        // SCYNn for NBG2/3, 11-bit value). RBG layers use rotation params
        // instead -- left at 0 here.
        int         scrollX          = 0;
        int         scrollY          = 0;

        // Zoom factor for NBG0/1 (decoded from ZMXNn / ZMYNn). NBG2/3
        // do not support zoom; RBG uses rotation params. Default 1.0.
        // Formula mirrors VIDVulkan: zoom = 65536 / (reg & 0x7FF00).
        float       zoomX            = 1.0f;
        float       zoomY            = 1.0f;

        // Mosaic enable (MZCTL bit per layer).
        bool        mosaicEnabled    = false;

        // Color-calc ratio for this layer (CCRNA/B/CCRR 5-bit field).
        // 0 = 0/32 (full opacity blend), 31 = 31/32.
        int         colorCalcRatio   = 0;

        // Color offset select. CLOFEN bit per layer gates enable; CLOFSL
        // bit selects A vs B. -1 = disabled, 0 = offset set A, 1 = set B.
        int         colorOffsetSel   = -1;
        // Decoded R/G/B offset values from COA*/COB* (-256..255 signed 9-bit).
        int         colorOffsetR     = 0;
        int         colorOffsetG     = 0;
        int         colorOffsetB     = 0;
    };
    std::array<LayerInfo, 6> layers{};   // [0]=NBG0,[1]=NBG1,[2]=NBG2,[3]=NBG3,[4]=RBG0,[5]=RBG1

    // -------- Capture --------
    //
    // Any pointer arg may be nullptr to signal "skip this region".
    static Vdp2Snapshot takeRaw(const Vdp2&                          regs,
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
                                uint64_t                             frameId);

    // Fill layers[] from regs / fixRegs. Pure function: no globals, no
    // Vulkan, safe to unit test without the emulation core.
    void decodeLayers();
};
