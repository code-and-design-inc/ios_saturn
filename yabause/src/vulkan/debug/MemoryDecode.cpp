// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
// ASCII-only. See MemoryDecode.h header comment.
#include "MemoryDecode.h"

namespace memdecode {

Vdp1ColorDecoded decodeVdp1Color(uint32_t rgba8_le) {
    const uint8_t r = static_cast<uint8_t>( rgba8_le        & 0xFFu);
    const uint8_t g = static_cast<uint8_t>((rgba8_le >>  8) & 0xFFu);
    const uint8_t b = static_cast<uint8_t>((rgba8_le >> 16) & 0xFFu);
    const uint8_t a = static_cast<uint8_t>((rgba8_le >> 24) & 0xFFu);
    Vdp1ColorDecoded d{};
    d.c             = static_cast<uint8_t>((a >> 6) & 0x1u);
    d.colorcl       = static_cast<uint8_t>((a >> 3) & 0x7u);
    d.priority      = static_cast<uint8_t>( a       & 0x7u);
    d.shadow        = static_cast<uint8_t>((b >> 7) & 0x1u);
    d.sprite_window = static_cast<uint8_t>((b >> 6) & 0x1u);
    d.colorindex    = static_cast<uint16_t>((static_cast<uint16_t>(g) << 8) | r);
    return d;
}

std::vector<uint16_t> readLut(const uint8_t* vram, size_t vramSize,
                              uint32_t lutAddr, uint32_t entryCount) {
    std::vector<uint16_t> out(entryCount, 0);
    for (uint32_t i = 0; i < entryCount; ++i) {
        const size_t hi = static_cast<size_t>(lutAddr) + i * 2;
        const size_t lo = hi + 1;
        if (lo >= vramSize) continue;
        out[i] = static_cast<uint16_t>(
            (static_cast<uint16_t>(vram[hi]) << 8) | vram[lo]);
    }
    return out;
}

namespace {

inline uint8_t expand5To8(uint8_t v5) {
    // Saturn 5-bit channel -> 8-bit: value * 8 (matches shader packDirect's
    // value*8/255 path * 255).
    return static_cast<uint8_t>((v5 & 0x1Fu) * 8u);
}

inline uint32_t saturnRgb555ToRgba8(uint16_t w) {
    const uint8_t r = expand5To8(static_cast<uint8_t>( w        & 0x1Fu));
    const uint8_t g = expand5To8(static_cast<uint8_t>((w >>  5) & 0x1Fu));
    const uint8_t b = expand5To8(static_cast<uint8_t>((w >> 10) & 0x1Fu));
    return (static_cast<uint32_t>(0xFFu) << 24) |
           (static_cast<uint32_t>(b)     << 16) |
           (static_cast<uint32_t>(g)     <<  8) |
            static_cast<uint32_t>(r);
}

inline uint16_t readBeWord(const uint8_t* vram, size_t vramSize, size_t addr) {
    if (addr + 1 >= vramSize) return 0;
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(vram[addr]) << 8) | vram[addr + 1]);
}

inline uint32_t cramLookupRgba(const uint8_t* cram, size_t cramSize,
                                uint16_t colorindex) {
    const size_t addr = static_cast<size_t>(colorindex) * 2;
    if (addr + 1 >= cramSize) return 0;
    const uint16_t w = static_cast<uint16_t>(
        (static_cast<uint16_t>(cram[addr]) << 8) | cram[addr + 1]);
    return saturnRgb555ToRgba8(w);
}

}  // namespace

std::vector<uint32_t> decode4bppLut(
    const uint8_t* vram, size_t vramSize,
    uint32_t srcAddr, uint32_t w, uint32_t h,
    uint32_t lutAddr,
    const uint8_t* cram, size_t cramSize,
    bool spd, bool endcEnabled) {
    std::vector<uint32_t> out(static_cast<size_t>(w) * h, 0);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const uint32_t byteIdx = (y * w + x) >> 1;
            const size_t   addr    = static_cast<size_t>(srcAddr) + byteIdx;
            if (addr >= vramSize) continue;
            const uint8_t b = vram[addr];
            const uint8_t dot = ((x & 1u) == 0u)
                ? static_cast<uint8_t>((b >> 4) & 0xFu)
                : static_cast<uint8_t>( b       & 0xFu);
            if (endcEnabled && dot == 0xFu) continue;       // transparent
            if (dot == 0u && !spd)         continue;        // transparent
            const size_t lutEntryAddr =
                static_cast<size_t>(lutAddr) + static_cast<size_t>(dot) * 2;
            const uint16_t lutWord = readBeWord(vram, vramSize, lutEntryAddr);
            if (lutWord & 0x8000u) {
                out[y * w + x] = saturnRgb555ToRgba8(lutWord);
            } else {
                // Palette mode: index into CRAM at (lutWord & 0x7FF) * 2.
                const size_t cramAddr =
                    static_cast<size_t>(lutWord & 0x7FFu) * 2;
                if (cramAddr + 1 >= cramSize) continue;
                const uint16_t cramWord = static_cast<uint16_t>(
                    (static_cast<uint16_t>(cram[cramAddr]) << 8) |
                    cram[cramAddr + 1]);
                out[y * w + x] = saturnRgb555ToRgba8(cramWord);
            }
        }
    }
    return out;
}

std::vector<uint32_t> decode4bppBank(
    const uint8_t* vram, size_t vramSize,
    uint32_t srcAddr, uint32_t w, uint32_t h,
    uint16_t colorBank,
    const uint8_t* cram, size_t cramSize,
    bool spd, bool endcEnabled) {
    std::vector<uint32_t> out(static_cast<size_t>(w) * h, 0);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const uint32_t byteIdx = (y * w + x) >> 1;
            const size_t   addr    = static_cast<size_t>(srcAddr) + byteIdx;
            if (addr >= vramSize) continue;
            const uint8_t b   = vram[addr];
            const uint8_t dot = ((x & 1u) == 0u)
                ? static_cast<uint8_t>((b >> 4) & 0xFu)
                : static_cast<uint8_t>( b       & 0xFu);
            if (endcEnabled && dot == 0xFu) continue;
            if (dot == 0u && !spd)         continue;
            const uint16_t colorindex =
                static_cast<uint16_t>(static_cast<uint16_t>(dot) | colorBank);
            out[y * w + x] = cramLookupRgba(cram, cramSize, colorindex);
        }
    }
    return out;
}

std::vector<uint32_t> decode8bppBank(
    const uint8_t* vram, size_t vramSize,
    uint32_t srcAddr, uint32_t w, uint32_t h,
    uint16_t colorBank, uint8_t palMask,
    const uint8_t* cram, size_t cramSize,
    bool spd, bool endcEnabled) {
    std::vector<uint32_t> out(static_cast<size_t>(w) * h, 0);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const size_t addr = static_cast<size_t>(srcAddr) + y * w + x;
            if (addr >= vramSize) continue;
            const uint8_t dot = vram[addr];
            if (endcEnabled && (dot & palMask) == palMask) continue;
            if (dot == 0u && !spd)                          continue;
            const uint16_t colorindex = static_cast<uint16_t>(
                static_cast<uint16_t>(dot & palMask) | colorBank);
            out[y * w + x] = cramLookupRgba(cram, cramSize, colorindex);
        }
    }
    return out;
}

std::vector<uint32_t> decode16bppRGB(
    const uint8_t* vram, size_t vramSize,
    uint32_t srcAddr, uint32_t w, uint32_t h,
    bool endcEnabled) {
    std::vector<uint32_t> out(static_cast<size_t>(w) * h, 0);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const size_t addr = static_cast<size_t>(srcAddr) + (y * w + x) * 2;
            if (addr + 1 >= vramSize) continue;
            const uint16_t word = readBeWord(vram, vramSize, addr);
            if (endcEnabled && word == 0x7FFFu) continue;
            if ((word & 0x8000u) == 0u)        continue;  // not visible
            out[y * w + x] = saturnRgb555ToRgba8(word);
        }
    }
    return out;
}

}  // namespace memdecode
