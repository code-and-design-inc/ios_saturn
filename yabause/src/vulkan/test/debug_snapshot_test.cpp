// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
// TDD test for DebugSnapshot deep-copy semantics.
#include "gtest/gtest.h"
#include "../debug/DebugSnapshot.h"

#include <cstring>
#include <vector>

TEST(DebugSnapshot, TakeCopiesCmdsByValue) {
    std::vector<Vdp1Cmd> live;
    live.resize(2);
    live[0].cmdType = VDP1C_TYPE_POLYGON;
    live[1].cmdType = VDP1C_TYPE_DISTORTED_SPRITE;

    vdp1c::Vdp1State state{};
    std::vector<uint8_t> vram(512 * 1024, 0xAA);
    std::vector<uint8_t> cram(4 * 1024, 0x55);
    Vdp1 regs{};
    regs.TVMR = 0x1234;

    auto snap = DebugSnapshot::take(live, state,
                                     vram.data(), vram.size(),
                                     cram.data(), cram.size(),
                                     regs, 704, 512, 42);

    // Mutate originals. Snapshot must be unaffected.
    live[0].cmdType = VDP1C_TYPE_NOOP;
    EXPECT_EQ(snap.cmds[0].cmdType, VDP1C_TYPE_POLYGON);
    EXPECT_EQ(snap.cmds[1].cmdType, VDP1C_TYPE_DISTORTED_SPRITE);
}

TEST(DebugSnapshot, TakeCopiesVramAndCram) {
    std::vector<Vdp1Cmd> live;
    vdp1c::Vdp1State state{};
    std::vector<uint8_t> vram(512 * 1024, 0xAA);
    std::vector<uint8_t> cram(4 * 1024, 0x55);
    Vdp1 regs{};

    auto snap = DebugSnapshot::take(live, state,
                                     vram.data(), vram.size(),
                                     cram.data(), cram.size(),
                                     regs, 704, 512, 0);

    EXPECT_EQ(snap.vram.size(), 512u * 1024);
    EXPECT_EQ(snap.cram.size(), 4u * 1024);
    EXPECT_EQ(snap.vram[0], 0xAA);
    EXPECT_EQ(snap.cram[0], 0x55);

    // Mutate originals: snapshot must be a deep copy.
    vram[0] = 0x00;
    cram[0] = 0x00;
    EXPECT_EQ(snap.vram[0], 0xAA);
    EXPECT_EQ(snap.cram[0], 0x55);
}

TEST(DebugSnapshot, TakeStoresRegsAndDimensions) {
    std::vector<Vdp1Cmd> live;
    vdp1c::Vdp1State state{};
    std::vector<uint8_t> vram(512 * 1024);
    std::vector<uint8_t> cram(4 * 1024);
    Vdp1 regs{};
    regs.TVMR = 0x1234;
    regs.FBCR = 0x5678;

    auto snap = DebugSnapshot::take(live, state,
                                     vram.data(), vram.size(),
                                     cram.data(), cram.size(),
                                     regs, 704, 512, 12345);

    EXPECT_EQ(snap.regs.TVMR, 0x1234);
    EXPECT_EQ(snap.regs.FBCR, 0x5678);
    EXPECT_EQ(snap.fbWidth, 704);
    EXPECT_EQ(snap.fbHeight, 512);
    EXPECT_EQ(snap.frameId, 12345u);
}

namespace {
// Write a 0x20-byte VDP1 command at `off`. CMDCTRL bits 13-12 = 00 (jump =
// next), so the parser advances linearly. Only the fields the rawToCmdIndex
// builder inspects (CMDCTRL, CMDSIZE) are set.
void writeCmd(std::vector<uint8_t>& vram, size_t off,
              uint16_t cmdctrl, uint16_t cmdsize) {
    vram[off + 0] = (uint8_t)(cmdctrl >> 8);
    vram[off + 1] = (uint8_t)(cmdctrl & 0xFF);
    vram[off + 10] = (uint8_t)(cmdsize >> 8);
    vram[off + 11] = (uint8_t)(cmdsize & 0xFF);
}
}  // namespace

TEST(DebugSnapshot, RawToCmdIndexSkipsDegenerateDrawCommands) {
    // A degenerate draw command (here a zero-size normal sprite) is skipped by
    // the renderer's Draw method before it appends to cpuCmds. The raw->compute
    // index map must skip it too, otherwise every later command maps to the
    // wrong cpuCmds slot and the tail spills to -1 -- which the JSONL exporter
    // surfaces as a bogus "skipped_unsupported" on a command that actually drew.
    //
    // Layout:
    //   raw 0: normal sprite, valid size      -> appends cpuCmds[0]
    //   raw 1: normal sprite, CMDSIZE == 0     -> renderer bails, NO append
    //   raw 2: distorted sprite, valid size    -> appends cpuCmds[1]
    //   raw 3: draw end
    std::vector<uint8_t> vram(512 * 1024, 0);
    writeCmd(vram, 0x000, 0x0000, 0x0508);  // normal, 5x8 cells -> drawn
    writeCmd(vram, 0x020, 0x0000, 0x0000);  // normal, 0 size    -> skipped
    writeCmd(vram, 0x040, 0x0002, 0x0508);  // distorted, valid  -> drawn
    vram[0x060] = 0x80;                      // draw end

    // cpuCmds: two appended entries. Normal sprites encode internally as
    // DISTORTED_SPRITE (encodeNormalSprite -> encodeDistortedSprite).
    std::vector<Vdp1Cmd> live(2);
    live[0].cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    live[1].cmdType = VDP1C_TYPE_DISTORTED_SPRITE;

    vdp1c::Vdp1State state{};
    std::vector<uint8_t> cram(4 * 1024, 0);
    Vdp1 regs{};
    auto snap = DebugSnapshot::take(live, state,
                                     vram.data(), vram.size(),
                                     cram.data(), cram.size(),
                                     regs, 704, 512, 1);

    ASSERT_EQ(snap.rawCmds.size(), 3u);
    ASSERT_EQ(snap.rawToCmdIndex.size(), 3u);
    EXPECT_EQ(snap.rawToCmdIndex[0], 0);   // valid normal -> cpuCmds[0]
    EXPECT_EQ(snap.rawToCmdIndex[1], -1);  // degenerate normal -> not drawn
    EXPECT_EQ(snap.rawToCmdIndex[2], 1);   // distorted -> cpuCmds[1] (NOT -1)
}

TEST(DebugSnapshot, ParseRawCmdsHandlesEmpty) {
    std::vector<uint8_t> vram(512 * 1024, 0);
    // CMDCTRL big-endian: high byte first. 0x80 in high byte sets bit15
    // (Draw End). parseRawCmds must early-exit.
    vram[0] = 0x80;
    auto cmds = DebugSnapshot::parseRawCmds(vram.data(), vram.size());
    EXPECT_TRUE(cmds.empty());
}

TEST(DebugSnapshot, ParseRawCmdsHandlesEndAndLink) {
    // VRAM layout:
    //   0x000: command 0 - non-end, jump=next (00), terminates by reaching command 1
    //   0x020: command 1 - END (CMDCTRL bit15 set)
    // After parsing, rawCmds.size() must be exactly 1, and that one cmd
    // must hold the values we wrote.
    std::vector<uint8_t> vram(512 * 1024, 0);

    // Command 0 at offset 0x000.
    // CMDCTRL: bits 13-12 = 00 (jump = next), bit15 = 0 (not end).
    // Set CMDCTRL bits 3-0 = 0001 (Sprite-style, but parseRawCmds is opcode-agnostic).
    // Use a non-trivial sentinel so we can verify the value reached the snapshot.
    vram[0x000] = 0x00;  // CMDCTRL high byte
    vram[0x001] = 0x01;  // CMDCTRL low byte (== 0x0001)
    vram[0x002] = 0x00;  // CMDLINK high
    vram[0x003] = 0x00;  // CMDLINK low
    vram[0x004] = 0x12;  // CMDPMOD high  (== 0x1234)
    vram[0x005] = 0x34;
    vram[0x006] = 0xAB;  // CMDCOLR (== 0xABCD)
    vram[0x007] = 0xCD;

    // Command 1 at offset 0x020 - END marker (CMDCTRL bit15 set).
    vram[0x020] = 0x80;  // CMDCTRL high byte: bit15 set
    vram[0x021] = 0x00;

    auto cmds = DebugSnapshot::parseRawCmds(vram.data(), vram.size());
    ASSERT_EQ(cmds.size(), 1u);
    EXPECT_EQ(cmds[0].CMDCTRL, 0x0001);
    EXPECT_EQ(cmds[0].CMDPMOD, 0x1234);
    EXPECT_EQ(cmds[0].CMDCOLR, 0xABCD);
}
