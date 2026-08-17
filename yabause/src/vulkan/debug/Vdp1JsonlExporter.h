// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// Vdp1JsonlExporter: serializes a DebugSnapshot into the JSONL format
// described in the VDP1 Debug UI design spec (section 6).
//
// Each line is a self-contained JSON object: a snapshot_meta line followed
// by one command line per VDP1 command. singleCommandLine builds the
// per-command line; exportSnapshot writes the whole file in one go.
//
// NOTE: ASCII-only file. MSVC's default JP code page (cp932) corrupts
// multi-byte literals in source.
#pragma once

#include "DebugSnapshot.h"
#include <string>

class Vdp1JsonlExporter {
public:
    // Write the entire snapshot to a file. Returns true on success.
    // On failure, errorMsg (if non-null) is filled with a reason.
    static bool exportSnapshot(const DebugSnapshot& snap,
                               const std::string& path,
                               std::string* errorMsg);

    // Build the snapshot_meta line (no trailing newline).
    static std::string snapshotMetaLine(const DebugSnapshot& snap, int stepNAtExport);

    // Build a single command line (no trailing newline). Returns empty
    // string if index is out of range.
    static std::string singleCommandLine(const DebugSnapshot& snap, int index);
};
