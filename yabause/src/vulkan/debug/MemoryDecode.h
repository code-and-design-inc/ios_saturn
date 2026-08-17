// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// Pure-function decoders used by the FrameDebugger Memory Viewer panel.
// All functions are I/O-free and test-only depend on the GLM headers via
// their callers -- this file itself is plain C++17.
//
// NOTE: ASCII-only file. MSVC default JP code page (cp932) corrupts
// multi-byte literals in source.
#pragma once

#include <cstdint>
#include <vector>

namespace memdecode {

// Decode VDP1COLOR encoded in a 32-bit little-endian RGBA8 word
// (R at byte 0, G at byte 1, B at byte 2, alpha at byte 3 -- matches
// host CPU layout written by the compute shader's packPalette/packDirect).
// Layout per Vdp1ComputeRasterizer.cpp comment:
//   alpha byte: bit 7 = S (always 1 for valid pixel),
//               bit 6 = C (1 = palette, 0 = direct),
//               bits 5:3 = colorcl,
//               bits 2:0 = priority.
//   B byte:     bit 7 = shadow, bit 6 = sprite_window.
//   G byte:     colorindex >> 8.
//   R byte:     colorindex & 0xFF.
struct Vdp1ColorDecoded {
    uint8_t  c;              // 1 = palette index in (R, G), 0 = direct RGB
    uint8_t  colorcl;        // 0..7
    uint8_t  priority;       // 0..7
    uint8_t  shadow;         // 0/1
    uint8_t  sprite_window;  // 0/1
    uint16_t colorindex;     // 16-bit palette index (or RGB low 16 bits when c=0)
};

Vdp1ColorDecoded decodeVdp1Color(uint32_t rgba8_le);

// Read `entryCount` 16-bit words from `vram` starting at byte address
// `lutAddr`. Saturn VRAM is big-endian (high byte first), so each word's
// MSB is at vram[addr], LSB at vram[addr+1]. Out-of-range entries return 0.
std::vector<uint16_t> readLut(const uint8_t* vram, size_t vramSize,
                              uint32_t lutAddr, uint32_t entryCount);

// 4bpp LUT mode texture decoder.
// - Reads `w*h/2` bytes from `vram[srcAddr ..]` as packed 4-bit indices
//   (high nibble first, mirrors shader sample4bppLut layout).
// - For each dot, looks up `vram[lutAddr + dot*2]` as a 16-bit big-endian
//   LUT word. If word bit 15 = 1, treats it as RGB direct (5/5/5 -> 8/8/8).
//   If word bit 15 = 0, treats it as a CRAM palette index -- `cram` is
//   indexed at `(word & 0x7FF) * 2` for 5/5/5 RGB.
// - dot==0 with !spd -> transparent (alpha=0).
// - dot==0xF with endcEnabled -> transparent (alpha=0).
// Returns RGBA8 (host little-endian) of size w*h. Out-of-range pixels
// produce alpha=0 transparent.
std::vector<uint32_t> decode4bppLut(
    const uint8_t* vram, size_t vramSize,
    uint32_t srcAddr, uint32_t w, uint32_t h,
    uint32_t lutAddr,
    const uint8_t* cram, size_t cramSize,
    bool spd, bool endcEnabled);

// 4bpp Bank mode. dot 4-bit OR'd with `colorBank` (CMDCOLR & 0xFFF0)
// gives a 12-bit palette index into CRAM (CRAM word = 5/5/5 RGB).
std::vector<uint32_t> decode4bppBank(
    const uint8_t* vram, size_t vramSize,
    uint32_t srcAddr, uint32_t w, uint32_t h,
    uint16_t colorBank,
    const uint8_t* cram, size_t cramSize,
    bool spd, bool endcEnabled);

// 8bpp Bank mode. dot 8-bit `& palMask` OR'd with `colorBank`.
// palMask is 0x3F (cm=2), 0x7F (cm=3), or 0xFF (cm=4).
// End-code triggers when (dot & palMask) == palMask.
std::vector<uint32_t> decode8bppBank(
    const uint8_t* vram, size_t vramSize,
    uint32_t srcAddr, uint32_t w, uint32_t h,
    uint16_t colorBank, uint8_t palMask,
    const uint8_t* cram, size_t cramSize,
    bool spd, bool endcEnabled);

// 16bpp RGB direct mode. Each pixel is one big-endian 5/5/5 word in vram.
// word == 0x7FFF is end-code. Pixels with bit 15 = 0 are transparent.
std::vector<uint32_t> decode16bppRGB(
    const uint8_t* vram, size_t vramSize,
    uint32_t srcAddr, uint32_t w, uint32_t h,
    bool endcEnabled);

}  // namespace memdecode
