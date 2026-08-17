// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
// TDD test for MemoryDecode pure functions. ASCII-only.
#include "gtest/gtest.h"
#include "../debug/MemoryDecode.h"

TEST(MemoryDecode, DecodeVdp1ColorPaletteFields) {
    // Build a palette pixel: alpha = 0x80 | C(1<<6) | colorcl(2<<3) | priority(3)
    //                       = 0x80 | 0x40 | 0x10 | 0x03 = 0xD3
    // B byte = shadow(0) | sprite_window(0) = 0x00
    // G byte = colorindex >> 8 = 0x03
    // R byte = colorindex & 0xFF = 0x5C
    // -> palette index 0x035C, alpha=0xD3, B=0x00
    // RGBA8 packed little-endian: 0xD3_00_03_5C  (alpha hi, R lo)
    uint32_t rgba = (0xD3u << 24) | (0x00u << 16) | (0x03u << 8) | 0x5Cu;
    auto d = memdecode::decodeVdp1Color(rgba);
    EXPECT_EQ(d.c, 1u);
    EXPECT_EQ(d.colorcl, 2u);
    EXPECT_EQ(d.priority, 3u);
    EXPECT_EQ(d.shadow, 0u);
    EXPECT_EQ(d.sprite_window, 0u);
    EXPECT_EQ(d.colorindex, 0x035Cu);
}

TEST(MemoryDecode, DecodeVdp1ColorDirectColorAndShadow) {
    // C=0 (direct), colorcl=0, priority=4, shadow=1, sprite_window=0,
    // RGB low 16 bits = 0x7C1F.
    // alpha = 0x80 | 0x00 | 0x00 | 0x04 = 0x84
    // B byte = 0x80 (shadow=1)
    // G:R = 0x7C, 0x1F
    uint32_t rgba = (0x84u << 24) | (0x80u << 16) | (0x7Cu << 8) | 0x1Fu;
    auto d = memdecode::decodeVdp1Color(rgba);
    EXPECT_EQ(d.c, 0u);
    EXPECT_EQ(d.priority, 4u);
    EXPECT_EQ(d.shadow, 1u);
    EXPECT_EQ(d.sprite_window, 0u);
    EXPECT_EQ(d.colorindex, 0x7C1Fu);
}

TEST(MemoryDecode, ReadLut4bpp16Entries) {
    std::vector<uint8_t> vram(64, 0);
    // Place 16 LUT entries at offset 0x10. Entry i = 0x8000 | i (visible bit + i).
    for (uint32_t i = 0; i < 16; ++i) {
        const uint16_t w = static_cast<uint16_t>(0x8000u | i);
        vram[0x10 + i * 2 + 0] = static_cast<uint8_t>(w >> 8);
        vram[0x10 + i * 2 + 1] = static_cast<uint8_t>(w & 0xFFu);
    }
    auto entries = memdecode::readLut(vram.data(), vram.size(), 0x10, 16);
    ASSERT_EQ(entries.size(), 16u);
    for (uint32_t i = 0; i < 16; ++i) {
        EXPECT_EQ(entries[i], 0x8000u | i) << "entry " << i;
    }
}

TEST(MemoryDecode, ReadLutOutOfRangeReturnsZero) {
    std::vector<uint8_t> vram(8, 0xAA);
    auto entries = memdecode::readLut(vram.data(), vram.size(), 0x100, 4);
    ASSERT_EQ(entries.size(), 4u);
    for (auto e : entries) EXPECT_EQ(e, 0u);
}

namespace {
// Helper: write one big-endian 16-bit word into vram.
void writeBeWord(std::vector<uint8_t>& vram, uint32_t addr, uint16_t w) {
    vram[addr]     = static_cast<uint8_t>(w >> 8);
    vram[addr + 1] = static_cast<uint8_t>(w & 0xFFu);
}
}  // namespace

TEST(MemoryDecode, Decode4bppLutBasic) {
    std::vector<uint8_t> vram(0x200, 0);
    std::vector<uint8_t> cram(0x1000, 0);
    // Texture 2x1 at vram[0x00]: dot0=0x1, dot1=0x2 packed in one byte 0x12.
    vram[0x00] = 0x12;
    // LUT at 0x100. LUT[0] = palette idx 0 (irrelevant). LUT[1] = direct
    // RGB 0x8000 | (R=31, G=0, B=0)  -> 0xFC1F (R lower bits per Saturn 5/5/5).
    // Saturn 5/5/5: bit14:10=B, bit9:5=G, bit4:0=R, bit15=visible.
    writeBeWord(vram, 0x100 + 1 * 2, 0x8000u | (31u << 0));   // LUT[1] = bright red
    writeBeWord(vram, 0x100 + 2 * 2, 0x8000u | (31u << 5));   // LUT[2] = bright green
    auto out = memdecode::decode4bppLut(
        vram.data(), vram.size(), /*srcAddr*/ 0x00, /*w*/ 2, /*h*/ 1,
        /*lutAddr*/ 0x100, cram.data(), cram.size(),
        /*spd*/ false, /*endcEnabled*/ true);
    ASSERT_EQ(out.size(), 2u);
    // dot=1 -> LUT[1] direct, R=31 -> 8-bit ~ 248. RGBA = (a, b, g, r).
    // packed little-endian uint32: low byte = R = 248.
    EXPECT_EQ(out[0] & 0xFFu, 248u);          // R
    EXPECT_EQ((out[0] >> 24) & 0xFFu, 0xFFu); // alpha (visible)
    // dot=2 -> LUT[2] direct, G=31 -> 248. low byte (R) = 0.
    EXPECT_EQ(out[1] & 0xFFu, 0u);
    EXPECT_EQ((out[1] >> 8) & 0xFFu, 248u);   // G
}

TEST(MemoryDecode, Decode4bppLutTransparentZero) {
    std::vector<uint8_t> vram(0x100, 0);
    std::vector<uint8_t> cram(0x1000, 0);
    // dot0=0 -> transparent (spd=false)
    vram[0x00] = 0x00;
    auto out = memdecode::decode4bppLut(
        vram.data(), vram.size(), 0x00, 1, 1, 0x80, cram.data(), cram.size(),
        /*spd*/ false, /*endcEnabled*/ true);
    EXPECT_EQ((out[0] >> 24) & 0xFFu, 0u);  // alpha = 0
}

TEST(MemoryDecode, Decode4bppLutEndCodeF) {
    std::vector<uint8_t> vram(0x100, 0);
    std::vector<uint8_t> cram(0x1000, 0);
    // dot0=0xF -> transparent (endcEnabled=true)
    vram[0x00] = 0xF0;
    auto out = memdecode::decode4bppLut(
        vram.data(), vram.size(), 0x00, 1, 1, 0x80, cram.data(), cram.size(),
        /*spd*/ true, /*endcEnabled*/ true);
    EXPECT_EQ((out[0] >> 24) & 0xFFu, 0u);  // alpha = 0
}

TEST(MemoryDecode, Decode4bppBankBasic) {
    std::vector<uint8_t> vram(0x100, 0);
    std::vector<uint8_t> cram(0x1000, 0);
    // 1x1 texture, dot=1 (high nibble of byte 0).
    vram[0x00] = 0x10;
    // colorBank = 0x100 -> palette index 0x100 | 1 = 0x101.
    // Place a CRAM entry at index 0x101 -> byte addr 0x202: bright red.
    cram[0x202] = 0x00; cram[0x203] = 0x1F;  // BE word 0x001F (R=31)
    auto out = memdecode::decode4bppBank(
        vram.data(), vram.size(), 0x00, 1, 1,
        /*colorBank*/ 0x100, cram.data(), cram.size(),
        /*spd*/ true, /*endcEnabled*/ false);
    EXPECT_EQ(out[0] & 0xFFu, 248u);  // R = 31*8
}

TEST(MemoryDecode, Decode8bppBankPalMask) {
    std::vector<uint8_t> vram(0x100, 0);
    std::vector<uint8_t> cram(0x1000, 0);
    vram[0x00] = 0xC1;  // dot byte
    // cm=2 palMask=0x3F -> dot & 0x3F = 0x01. With colorBank=0x040 (& 0xFFC0
    // semantics) -> palette index 0x041.
    cram[0x082] = 0x03; cram[0x083] = 0xE0;  // BE 0x03E0 (G=31)
    auto out = memdecode::decode8bppBank(
        vram.data(), vram.size(), 0x00, 1, 1,
        /*colorBank*/ 0x40, /*palMask*/ 0x3F,
        cram.data(), cram.size(),
        /*spd*/ true, /*endcEnabled*/ false);
    EXPECT_EQ((out[0] >> 8) & 0xFFu, 248u);  // G channel
}

TEST(MemoryDecode, Decode8bppBankEndCodeMatchesMask) {
    std::vector<uint8_t> vram(0x100, 0);
    std::vector<uint8_t> cram(0x1000, 0);
    vram[0x00] = 0x3F;  // matches palMask 0x3F -> end-code
    auto out = memdecode::decode8bppBank(
        vram.data(), vram.size(), 0x00, 1, 1,
        0, 0x3F, cram.data(), cram.size(),
        /*spd*/ true, /*endcEnabled*/ true);
    EXPECT_EQ((out[0] >> 24) & 0xFFu, 0u);  // alpha = 0
}

TEST(MemoryDecode, Decode16bppRGBVisible) {
    std::vector<uint8_t> vram(0x100, 0);
    // 1x1 texture, BE word 0x8000 | (B=31 << 10) = 0xFC00 -> bright blue.
    vram[0x00] = 0xFC; vram[0x01] = 0x00;
    auto out = memdecode::decode16bppRGB(vram.data(), vram.size(), 0x00, 1, 1,
                                         /*endcEnabled*/ false);
    EXPECT_EQ((out[0] >> 16) & 0xFFu, 248u);  // B channel
}

TEST(MemoryDecode, Decode16bppRGBEndCode7FFF) {
    std::vector<uint8_t> vram(0x100, 0);
    vram[0x00] = 0x7F; vram[0x01] = 0xFF;  // 0x7FFF end-code
    auto out = memdecode::decode16bppRGB(vram.data(), vram.size(), 0x00, 1, 1,
                                         /*endcEnabled*/ true);
    EXPECT_EQ((out[0] >> 24) & 0xFFu, 0u);  // alpha = 0
}
