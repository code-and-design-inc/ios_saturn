// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// DebugSnapshot: freezes everything needed to reproduce/inspect a single
// VDP1 frame from the Pause UI.
//
// Captures (deep copy):
//   * compute side: Vdp1Cmd list + Vdp1State
//   * VDP1 VRAM (typically 512KB)
//   * VDP2 CRAM (4KB)
//   * Vdp1 register block
//   * raw command list parsed from VRAM (vdp1cmd_struct)
//
// All fields are owned by the snapshot, so the original buffers can mutate
// freely after take() returns.
//
// NOTE: ASCII-only file. MSVC's default JP code page (cp932) corrupts
// multi-byte literals in source.
#pragma once

#include "../Vdp1ComputeCommands.h"
#include "../Vdp1ComputeMath.h"

#include <vector>
#include <cstdint>
#include <cstddef>

extern "C" {
#include "vdp1.h"
}

struct DebugSnapshot {
    uint64_t frameId     = 0;
    uint64_t timestampMs = 0;
    int      fbWidth     = 0;
    int      fbHeight    = 0;

    std::vector<Vdp1Cmd>          cmds;
    vdp1c::Vdp1State              state;

    std::vector<uint8_t>          vram;
    std::vector<uint8_t>          cram;

    Vdp1                          regs{};
    std::vector<vdp1cmd_struct>   rawCmds;

    // Map rawCmds index -> compute cmds index, or -1 if the raw command
    // is not represented in the compute command list (state-update cmds
    // like LocalCoord/SystemClip/UserClip, draw_end, skipped, or types
    // not yet implemented in the compute path). Same length as rawCmds.
    // Built by take() after parseRawCmds() walks VRAM.
    std::vector<int32_t>          rawToCmdIndex;

    // Capture a snapshot. All pointer-data is deep copied.
    static DebugSnapshot take(const std::vector<Vdp1Cmd>& computeCmds,
                              const vdp1c::Vdp1State&     computeState,
                              const uint8_t*              vdp1Ram,
                              size_t                      vdp1RamSize,
                              const uint8_t*              vdp2ColorRam,
                              size_t                      vdp2ColorRamSize,
                              const Vdp1&                 vdp1Regs,
                              int                         fbW,
                              int                         fbH,
                              uint64_t                    frameId);

    // Inline VDP1 RAM walker. Stops at draw-end (CMDCTRL bit15 set) or
    // when the bits3-2 of CMDCTRL == 11 (table end). Does not call
    // Vdp1ReadCommand() so test executables don't need to link the C core.
    static std::vector<vdp1cmd_struct> parseRawCmds(const uint8_t* vdp1Ram,
                                                     size_t         vdp1RamSize);
};
