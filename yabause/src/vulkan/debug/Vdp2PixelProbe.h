// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// Vdp2PixelProbe: debugger-owned, self-contained VDP2 background-layer pixel
// decoder. Reads one Saturn pixel directly from frozen VDP2 registers + VRAM +
// CRAM without touching any renderer state or calling into vidsoft.
//
// Design decisions:
//   - Tile (cell) mode: faithfully decoded for NBG0-3 (4bpp/8bpp/16bpp palette
//     and 16bpp RGB555 color modes). Zoom (NBG0/1) and scroll are applied.
//   - Unsupported cases (bitmap, 32bpp direct, RBG0 pixel, etc.) set
//     supported=false with a short note. No fabricated colors.
//   - Sprite (layers[5]) is always supported=false because the VDP1 framebuffer
//     is GPU-side at pause time; caller shows "n/a".
//   - Color-calc state is decoded from Vdp2ColorCalcState.h (kNBG0..kRBG0).
//   - Priority tiebreak matches Vdp2Compositor.cpp: for priority 7..1, inner
//     loop runs which = kSprite(5) .. kNBG3(0), so higher layer-id wins ties.
//
// ASCII-only source (CLAUDE.md rule: MSVC CP932 misreads UTF-8 -> C4819/C2065).
#pragma once

#include <cstdint>
#include <cstring>

extern "C" {
#include "vdp2.h"
}

// ---------------------------------------------------------------------------
// Per-layer decoded pixel result.
// ---------------------------------------------------------------------------
struct Vdp2LayerProbe {
    const char* name      = "";     // "NBG0".."RBG0", "Sprite"
    bool  supported       = false;  // pixel color was faithfully decoded
    bool  enabled         = false;  // layer enabled in BGON
    bool  opaque          = false;  // non-transparent at (x,y)
    int   priority        = 0;      // 0..7 effective
    uint32_t color        = 0;      // 0x00RRGGBB, valid only if supported && opaque
    bool  ccEnable        = false;
    int   ccRatio         = 0;      // 0..63
    bool  isRotation      = false;
    char  note[48]        = {0};    // reason when !supported, or extra info
};

// ---------------------------------------------------------------------------
// Full per-pixel report.
// ---------------------------------------------------------------------------
struct Vdp2PixelReport {
    int x = 0, y = 0;
    // [0]=NBG0, [1]=NBG1, [2]=NBG2, [3]=NBG3, [4]=RBG0, [5]=Sprite
    Vdp2LayerProbe layers[6];
    // Highest-priority opaque supported layer among layers[0..4].
    // -1 if none. Tiebreak: higher layer-index wins (matches compositor).
    int topIdx    = -1;
    int secondIdx = -1;
    // Back screen color 0x00RRGGBB from BKTAU/BKTAL -> VRAM -> CRAM.
    // 0 if not decoded.
    uint32_t backColor = 0;
};

// ---------------------------------------------------------------------------
// Decode one Saturn pixel from frozen register / VRAM / CRAM state.
// All pointer data is read-only; no globals are written.
//
// Parameters:
//   x, y   -- Saturn pixel coordinates (0-based, logical resolution)
//   regs   -- frozen Vdp2 register struct (from Vdp2Snapshot::regs)
//   vram   -- pointer to 512KB VDP2 VRAM (Vdp2Snapshot::vdp2Ram.data())
//   cram   -- pointer to 4KB VDP2 color RAM (Vdp2Snapshot::cram.data())
// ---------------------------------------------------------------------------
Vdp2PixelReport probeVdp2Pixel(int x, int y,
                               const Vdp2&    regs,
                               const uint8_t* vram,
                               const uint8_t* cram);
