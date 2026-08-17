// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// DebugSnapshot implementation. See DebugSnapshot.h for design notes.
//
// NOTE: ASCII-only. parseRawCmds() is intentionally inline (not delegated
// to Vdp1ReadCommand) so that the unit test executable does not need to
// link the C emulation core.
#include "DebugSnapshot.h"

#include <chrono>
#include <cstring>

namespace {

inline uint16_t readWordBE(const uint8_t* base, size_t addr) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(base[addr])     << 8) |
         static_cast<uint16_t>(base[addr + 1])
    );
}

// Does a draw command (CMDCTRL low nibble 0..6) actually append a Vdp1Cmd in
// compute mode? Each draw type normally appends exactly one command, EXCEPT
// when its Draw method early-returns before appendCommand(). Those skipped
// commands must NOT advance the compute index, otherwise every later command
// maps to the wrong cpuCmds slot and the tail spills to -1 (surfacing as a
// bogus "skipped_unsupported" in the JSONL on commands that actually drew).
//
// Predicates mirror the compute-mode early-returns in Vdp1Renderer.cpp:
//   Normal   (0)  : bail if CMDSIZE bad-bit (0x8000) set, or char w/h == 0
//   Scaled   (1)  : never bails (0-size falls back to a 1x1 pattern)
//   Distorted(2,3): bail if char w/h == 0 (covers CMDSIZE == 0)
//   Polygon  (4)  : never bails
//   Polyline (5,7): never bails
//   Line     (6)  : never bails
// CMDSIZE is only meaningful for sprite types (0,1,2,3); the non-sprite types
// return true regardless, so garbage in those slots is irrelevant.
bool computeAppendsCmd(const vdp1cmd_struct& rc) {
    const int charW = (rc.CMDSIZE >> 8) & 0x3F;  // char cells wide
    const int charH =  rc.CMDSIZE       & 0xFF;  // char height (px)
    switch (rc.CMDCTRL & 0x000F) {
    case 0:  return !(rc.CMDSIZE & 0x8000) && charW != 0 && charH != 0;
    case 1:  return true;
    case 2:
    case 3:  return charW != 0 && charH != 0;
    case 4:
    case 5:
    case 6:
    case 7:  return true;
    default: return false;  // 8/9/10 mutate state (no append); 11+ bad
    }
}

}  // namespace

DebugSnapshot DebugSnapshot::take(const std::vector<Vdp1Cmd>& computeCmds,
                                   const vdp1c::Vdp1State&     computeState,
                                   const uint8_t*              vdp1Ram,
                                   size_t                      vdp1RamSize,
                                   const uint8_t*              vdp2ColorRam,
                                   size_t                      vdp2ColorRamSize,
                                   const Vdp1&                 vdp1Regs,
                                   int                         fbW,
                                   int                         fbH,
                                   uint64_t                    frameId)
{
    DebugSnapshot s;
    s.frameId = frameId;
    auto now = std::chrono::system_clock::now().time_since_epoch();
    s.timestampMs = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count()
    );
    s.fbWidth  = fbW;
    s.fbHeight = fbH;

    s.cmds  = computeCmds;
    s.state = computeState;

    if (vdp1Ram != nullptr && vdp1RamSize > 0) {
        s.vram.assign(vdp1Ram, vdp1Ram + vdp1RamSize);
    }
    if (vdp2ColorRam != nullptr && vdp2ColorRamSize > 0) {
        s.cram.assign(vdp2ColorRam, vdp2ColorRam + vdp2ColorRamSize);
    }

    s.regs    = vdp1Regs;
    s.rawCmds = parseRawCmds(s.vram.data(), s.vram.size());

    // Build rawCmds index -> cmds index mapping. The compute path appends
    // a Vdp1Cmd for every draw command (CMDCTRL low nibble 0..6):
    //   0 = Normal Sprite, 1 = Scaled Sprite, 2/3 = Distorted Sprite,
    //   4 = Polygon, 5 = Polyline, 6 = Line.
    // Plan 1 only mapped 4/2/3 (Polygon + Distorted) since those were the
    // only types F11+earlier hooked into compute. F12 added the other four
    // hooks, so the mapping must keep up -- otherwise step replay translates
    // raw indices into wrong cpuCmds slots and the offscreen preview lags
    // by exactly the number of skipped types (e.g. NiGHTS title: 136
    // normal_sprite cmds at the front of rawCmds got mapped to -1, and the
    // first polygon at raw idx ~139 mapped to cpuCmds[0] instead of [136]).
    // User/System Clipping and Local Coord (types 8/9/10) get -1 because they
    // mutate state but are NOT pushed to cpuCmds. Degenerate draw commands
    // (e.g. a zero-size sprite) also get -1: the renderer's Draw method bails
    // before appendCommand(), so they must not consume a cpuCmds slot --
    // see computeAppendsCmd() for the per-type predicate. Skipping them keeps
    // the running index aligned with cpuCmds; otherwise the tail of drawn
    // commands maps to -1 and the JSONL exporter reports them as
    // "skipped_unsupported" even though they were drawn.
    s.rawToCmdIndex.assign(s.rawCmds.size(), -1);
    int kCompute = 0;
    for (size_t i = 0; i < s.rawCmds.size(); ++i) {
        const uint16_t ctrl = s.rawCmds[i].CMDCTRL;
        const bool drawEnd = (ctrl & 0x8000) != 0;
        const bool skipped = (ctrl & 0x4000) != 0;
        const bool isComputeDrawn =
            !drawEnd && !skipped && computeAppendsCmd(s.rawCmds[i]);
        if (isComputeDrawn && kCompute < (int)s.cmds.size()) {
            s.rawToCmdIndex[i] = kCompute++;
        }
    }

    return s;
}

std::vector<vdp1cmd_struct> DebugSnapshot::parseRawCmds(const uint8_t* vdp1Ram,
                                                         size_t         vdp1RamSize)
{
    std::vector<vdp1cmd_struct> out;
    out.reserve(256);

    if (vdp1Ram == nullptr || vdp1RamSize < 0x20) return out;

    constexpr size_t kInvalid = static_cast<size_t>(-1);
    constexpr size_t kCmdSize = 0x20;
    constexpr size_t kMaxCmds = 8192;
    constexpr size_t kAddrCap = 0x7FFE0;

    size_t addr       = 0;
    size_t returnAddr = kInvalid;

    while (addr + kCmdSize <= vdp1RamSize) {
        uint16_t command = readWordBE(vdp1Ram, addr);

        // Bit 15 set: end of command list (Draw End).
        if (command & 0x8000) break;

        // Bit 3-2 == 11: end-of-table marker (some drivers).
        if ((command & 0x000C) == 0x000C) break;

        vdp1cmd_struct c{};
        c.CMDCTRL = readWordBE(vdp1Ram, addr +  0);
        c.CMDLINK = readWordBE(vdp1Ram, addr +  2);
        c.CMDPMOD = readWordBE(vdp1Ram, addr +  4);
        c.CMDCOLR = readWordBE(vdp1Ram, addr +  6);
        c.CMDSRCA = readWordBE(vdp1Ram, addr +  8);
        c.CMDSIZE = readWordBE(vdp1Ram, addr + 10);
        c.CMDXA   = static_cast<int16_t>(readWordBE(vdp1Ram, addr + 12));
        c.CMDYA   = static_cast<int16_t>(readWordBE(vdp1Ram, addr + 14));
        c.CMDXB   = static_cast<int16_t>(readWordBE(vdp1Ram, addr + 16));
        c.CMDYB   = static_cast<int16_t>(readWordBE(vdp1Ram, addr + 18));
        c.CMDXC   = static_cast<int16_t>(readWordBE(vdp1Ram, addr + 20));
        c.CMDYC   = static_cast<int16_t>(readWordBE(vdp1Ram, addr + 22));
        c.CMDXD   = static_cast<int16_t>(readWordBE(vdp1Ram, addr + 24));
        c.CMDYD   = static_cast<int16_t>(readWordBE(vdp1Ram, addr + 26));
        c.CMDGRDA = readWordBE(vdp1Ram, addr + 28);
        out.push_back(c);

        // Bits 13-12 of CMDCTRL: jump mode.
        //   00 = next (addr += 0x20)
        //   01 = assign (jump to CMDLINK*8)
        //   10 = call   (push return addr, jump)
        //   11 = return (pop return addr)
        switch ((command & 0x3000) >> 12) {
        case 0:
            addr += kCmdSize;
            break;
        case 1:
            addr = static_cast<size_t>(readWordBE(vdp1Ram, addr + 2)) * 8;
            break;
        case 2:
            if (returnAddr == kInvalid)
                returnAddr = addr + kCmdSize;
            addr = static_cast<size_t>(readWordBE(vdp1Ram, addr + 2)) * 8;
            break;
        case 3:
            if (returnAddr != kInvalid) {
                addr       = returnAddr;
                returnAddr = kInvalid;
            } else {
                addr += kCmdSize;
            }
            break;
        }

        if (addr > kAddrCap) break;
        if (out.size() >= kMaxCmds) break;
    }
    return out;
}
