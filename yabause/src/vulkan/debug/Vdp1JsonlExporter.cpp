// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
//
// Vdp1JsonlExporter implementation. See Vdp1JsonlExporter.h for design
// notes. Task 8 fills in snapshotMetaLine and exportSnapshot.
//
// NOTE: ASCII-only file. MSVC's default JP code page (cp932) corrupts
// multi-byte literals in source.
#include "Vdp1JsonlExporter.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace {

std::string hexU16(uint16_t v) {
    std::ostringstream os;
    os << "\"0x" << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << v << "\"";
    return os.str();
}

std::string hexU32(uint32_t v) {
    std::ostringstream os;
    os << "\"0x" << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << v << "\"";
    return os.str();
}

const char* commandTypeStringFromRaw(uint16_t cmdctrl) {
    if (cmdctrl & 0x8000) return "draw_end";
    switch (cmdctrl & 0x000F) {
    case 0:  return "normal_sprite";
    case 1:  return "scaled_sprite";
    case 2:  return "distorted_sprite";
    case 3:  return "distorted_sprite";
    case 4:  return "polygon";
    case 5:  return "polyline";
    case 6:  return "line";
    case 7:  return "polyline";
    case 8:  return "user_clip_coords";
    case 9:  return "system_clip_coords";
    case 10: return "local_coordinates";
    default: return "bad_command";
    }
}

const char* colorModeString(uint16_t pmod) {
    switch ((pmod >> 3) & 0x7) {
    case 0: return "4bpp_bank";
    case 1: return "4bpp_lut";
    case 2: return "8bpp_64bank";
    case 3: return "8bpp_128bank";
    case 4: return "8bpp_256bank";
    case 5: return "16bpp_rgb";
    default: return "unknown";
    }
}

const char* colorCalcModeString(uint16_t pmod) {
    switch (pmod & 0x7) {
    case 0: return "replace";
    case 1: return "shadow";
    case 2: return "half_luminance";
    case 3: return "half_transparent";
    case 4: return "gouraud";
    case 6: return "gouraud_half_luminance";
    case 7: return "gouraud_half_transparent";
    default: return "unknown";
    }
}

bool isSpriteType(const char* cmdType) {
    return std::string(cmdType) == "normal_sprite" ||
           std::string(cmdType) == "scaled_sprite" ||
           std::string(cmdType) == "distorted_sprite";
}

bool isComputeSupported(uint32_t computeType) {
    // Plan 2 F12: every non-NOOP cmd type is now compute-drawn. NORMAL/
    // SCALED sprites internally encode as VDP1C_TYPE_DISTORTED_SPRITE, so
    // those land in the DISTORTED branch automatically. Adding POLYLINE /
    // LINE keeps the JSONL exporter's outcome.status truthful (otherwise
    // they show as "skipped_unsupported" while actually being drawn).
    return computeType != VDP1C_TYPE_NOOP;
}

std::string outcomeStatus(const char* cmdType, uint32_t computeType, uint16_t cmdctrl) {
    if (cmdctrl & 0x8000) return "draw_end";
    if (cmdctrl & 0x4000) return "skipped_by_skip_bit";
    std::string t = cmdType;
    if (t == "user_clip_coords" || t == "system_clip_coords" || t == "local_coordinates")
        return "applied_state";
    if (t == "bad_command") return "bad_command";
    if (isComputeSupported(computeType)) return "drawn";
    return "skipped_unsupported";
}

// Which of the four VDP1 coordinate pairs (A,B,C,D) a command type actually
// reads. The VDP1 command table is a fixed 32-byte record, but only some
// coordinate words are defined per command type; the rest hold whatever the
// game left in VRAM (often stale garbage). Emitting those stale words as
// "coordinates" in the raw block is misleading -- e.g. a normal_sprite only
// uses A (CMDXA/CMDYA) yet CMDXB..CMDYD print arbitrary out-of-range values.
// Returns a 4-bit mask: bit0=A, bit1=B, bit2=C, bit3=D. Verified against the
// renderer's own field usage in Vdp1Renderer.cpp (Normal/Scaled/Distorted/
// Polygon/Polyline/Line and clip/local-coordinate draw paths).
uint8_t usedCoordMask(uint16_t cmdctrl) {
    if (cmdctrl & 0x8000) return 0;  // draw_end
    switch (cmdctrl & 0x000F) {
    case 0:  return 0x1;             // normal sprite: A only
    case 1: {                        // scaled sprite: A + (C if zoom point 0 else B)
        uint16_t zoom = (cmdctrl & 0x0F00) >> 8;
        return (zoom == 0) ? (0x1 | 0x4) : (0x1 | 0x2);
    }
    case 2:
    case 3:  return 0xF;             // distorted sprite: A,B,C,D
    case 4:  return 0xF;             // polygon: A,B,C,D
    case 5:
    case 7:  return 0xF;             // polyline: A,B,C,D
    case 6:  return 0x1 | 0x2;       // line: A,B
    case 8:  return 0x1 | 0x4;       // user clip coords: A (upper-left), C (lower-right)
    case 9:  return 0x4;             // system clip coords: C (lower-right)
    case 10: return 0x1;             // local coordinates: A
    default: return 0;               // bad command
    }
}

void writeRawBlock(std::ostringstream& os, const vdp1cmd_struct& rc) {
    const uint8_t used = usedCoordMask(rc.CMDCTRL);
    auto coord = [&os](bool valid, int v) {
        if (valid) os << v; else os << "null";
    };
    os << "\"raw\": {";
    os << "\"CMDCTRL\": " << hexU16(rc.CMDCTRL) << ", ";
    os << "\"CMDLINK\": " << hexU16(rc.CMDLINK) << ", ";
    os << "\"CMDPMOD\": " << hexU16(rc.CMDPMOD) << ", ";
    os << "\"CMDCOLR\": " << hexU16(rc.CMDCOLR) << ", ";
    os << "\"CMDSRCA\": " << hexU16(rc.CMDSRCA) << ", ";
    os << "\"CMDSIZE\": " << hexU16(rc.CMDSIZE) << ", ";
    os << "\"CMDXA\": "; coord(used & 0x1, rc.CMDXA);
    os << ", \"CMDYA\": "; coord(used & 0x1, rc.CMDYA); os << ", ";
    os << "\"CMDXB\": "; coord(used & 0x2, rc.CMDXB);
    os << ", \"CMDYB\": "; coord(used & 0x2, rc.CMDYB); os << ", ";
    os << "\"CMDXC\": "; coord(used & 0x4, rc.CMDXC);
    os << ", \"CMDYC\": "; coord(used & 0x4, rc.CMDYC); os << ", ";
    os << "\"CMDXD\": "; coord(used & 0x8, rc.CMDXD);
    os << ", \"CMDYD\": "; coord(used & 0x8, rc.CMDYD); os << ", ";
    os << "\"CMDGRDA\": " << hexU16(rc.CMDGRDA);
    os << "}";
}

void writeDecodedBlock(std::ostringstream& os, const vdp1cmd_struct& rc, const char* cmdType) {
    bool isDraw = !(rc.CMDCTRL & 0x0008);
    os << "\"decoded\": {";
    os << "\"jump_mode\": \"";
    switch ((rc.CMDCTRL & 0x3000) >> 12) {
        case 0: os << "next";   break;
        case 1: os << "assign"; break;
        case 2: os << "call";   break;
        case 3: os << "return"; break;
    }
    os << "\", ";
    os << "\"skip\": " << ((rc.CMDCTRL & 0x4000) ? "true" : "false") << ", ";
    if (isDraw) {
        os << "\"msb_set\": " << ((rc.CMDPMOD & 0x8000) ? "true" : "false") << ", ";
        os << "\"hss_enabled\": " << ((rc.CMDPMOD & 0x1000) ? "true" : "false") << ", ";
        os << "\"pre_clipping_enabled\": " << ((rc.CMDPMOD & 0x0800) ? "false" : "true") << ", ";
        os << "\"user_clipping_enabled\": " << ((rc.CMDPMOD & 0x0400) ? "true" : "false") << ", ";
        if (rc.CMDPMOD & 0x0400) {
            os << "\"user_clipping_mode\": " << ((rc.CMDPMOD >> 9) & 1) << ", ";
        } else {
            os << "\"user_clipping_mode\": null, ";
        }
        os << "\"mesh_enabled\": " << ((rc.CMDPMOD & 0x0100) ? "true" : "false") << ", ";
        os << "\"ecd_enabled\": " << ((rc.CMDPMOD & 0x0080) ? "false" : "true") << ", ";
        os << "\"spd_enabled\": " << ((rc.CMDPMOD & 0x0040) ? "false" : "true") << ", ";
        os << "\"color_mode\": \"" << colorModeString(rc.CMDPMOD) << "\", ";
        os << "\"color_calc_mode\": \"" << colorCalcModeString(rc.CMDPMOD) << "\", ";
        bool gouraud = (rc.CMDPMOD & 0x4) != 0;
        os << "\"gouraud_enabled\": " << (gouraud ? "true" : "false") << ", ";
        if (gouraud) {
            os << "\"gouraud_table_addr\": " << hexU32(((uint32_t)rc.CMDGRDA) << 3) << ", ";
        } else {
            os << "\"gouraud_table_addr\": null, ";
        }
    } else {
        os << "\"msb_set\": null, \"hss_enabled\": null, "
              "\"pre_clipping_enabled\": null, \"user_clipping_enabled\": null, "
              "\"user_clipping_mode\": null, \"mesh_enabled\": null, "
              "\"ecd_enabled\": null, \"spd_enabled\": null, "
              "\"color_mode\": null, \"color_calc_mode\": null, "
              "\"gouraud_enabled\": null, \"gouraud_table_addr\": null, ";
    }
    if (isSpriteType(cmdType)) {
        os << "\"texture_addr\": " << hexU32(((uint32_t)rc.CMDSRCA) << 3) << ", ";
        int tw = (rc.CMDSIZE & 0x3F00) >> 5;
        int th = rc.CMDSIZE & 0xFF;
        os << "\"texture_size\": [" << tw << ", " << th << "], ";
        os << "\"texture_read_dir\": \"";
        switch ((rc.CMDCTRL >> 4) & 3) {
            case 0: os << "normal"; break;
            case 1: os << "reversed_h"; break;
            case 2: os << "reversed_v"; break;
            case 3: os << "reversed_hv"; break;
        }
        os << "\"";
    } else {
        os << "\"texture_addr\": null, \"texture_size\": null, \"texture_read_dir\": null";
    }
    os << "}";
}

void writeStateBlock(std::ostringstream& os, const vdp1c::Vdp1State& st) {
    os << "\"state_at_command\": {";
    os << "\"system_clip\": [" << st.systemClip.x << ", " << st.systemClip.y << "], ";
    os << "\"user_clip\": [" << st.userClip.x << ", " << st.userClip.y
                            << ", " << st.userClip.z << ", " << st.userClip.w << "], ";
    os << "\"local_coord\": [" << st.localCoord.x << ", " << st.localCoord.y << "]";
    os << "}";
}

void writeComputeBlock(std::ostringstream& os, const Vdp1Cmd& c) {
    os << "\"compute\": {";
    os << "\"v0\": [" << c.v0.x << ", " << c.v0.y << "], ";
    os << "\"v1\": [" << c.v1.x << ", " << c.v1.y << "], ";
    os << "\"v2\": [" << c.v2.x << ", " << c.v2.y << "], ";
    os << "\"v3\": [" << c.v3.x << ", " << c.v3.y << "], ";
    os << "\"bbox\": [" << c.bbox.x << ", " << c.bbox.y << ", "
                       << c.bbox.z << ", " << c.bbox.w << "], ";
    os << "\"flip_horizontal\": " << ((c.flip & 1u) ? "true" : "false") << ", ";
    os << "\"flip_vertical\": "   << ((c.flip & 2u) ? "true" : "false") << ", ";
    os << "\"color_field_packed\": " << c.color << ", ";
    os << "\"char_size_pixels\": [" << c.charSize.x << ", " << c.charSize.y << "], ";
    os << "\"src_addr_words\": " << c.srca << ", ";
    os << "\"gouraud_addr_words\": " << c.gouraudAddr.x;
    os << "}";
}

}  // namespace

std::string Vdp1JsonlExporter::singleCommandLine(const DebugSnapshot& snap, int index) {
    if (index < 0 || (size_t)index >= snap.rawCmds.size()) return "";

    const vdp1cmd_struct& rc = snap.rawCmds[index];
    // rawCmds and cmds are not parallel: cmds only holds compute-drawn
    // entries (Polygon, Distorted Sprite). DebugSnapshot::take builds
    // rawToCmdIndex to translate raw[i] -> compute index (or -1 for
    // non-compute raw entries).
    const int cmdIdx = (size_t)index < snap.rawToCmdIndex.size()
                           ? snap.rawToCmdIndex[index]
                           : -1;
    static const Vdp1Cmd kEmptyCmd{};
    const Vdp1Cmd& c = (cmdIdx >= 0 && (size_t)cmdIdx < snap.cmds.size())
                           ? snap.cmds[cmdIdx]
                           : kEmptyCmd;
    const char* cmdType = commandTypeStringFromRaw(rc.CMDCTRL);

    std::ostringstream os;
    os << "{";
    os << "\"type\": \"command\", ";
    os << "\"index\": " << index << ", ";
    os << "\"command_type\": \"" << cmdType << "\", ";

    writeRawBlock(os, rc);          os << ", ";
    writeDecodedBlock(os, rc, cmdType); os << ", ";

    if (isComputeSupported(c.cmdType)) {
        writeComputeBlock(os, c);
        os << ", ";
    } else {
        os << "\"compute\": null, ";
    }

    writeStateBlock(os, snap.state);    os << ", ";
    os << "\"outcome\": {";
    os << "\"status\": \"" << outcomeStatus(cmdType, c.cmdType, rc.CMDCTRL) << "\", ";
    os << "\"drawn\": " << (isComputeSupported(c.cmdType) ? "true" : "false");
    os << "}";

    os << "}";
    return os.str();
}

std::string Vdp1JsonlExporter::snapshotMetaLine(const DebugSnapshot& snap, int stepNAtExport) {
    std::ostringstream os;

    auto isoTime = [](uint64_t ms) {
        time_t s = (time_t)(ms / 1000);
        struct tm tm_utc;
#ifdef _WIN32
        gmtime_s(&tm_utc, &s);
#else
        gmtime_r(&s, &tm_utc);
#endif
        char buf[64];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03lluZ",
                 tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
                 tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec,
                 (unsigned long long)(ms % 1000));
        return std::string(buf);
    };

    os << "{";
    os << "\"type\": \"snapshot_meta\", ";
    os << "\"schema_version\": 1, ";
    os << "\"exporter\": \"yabasanshiro-vulkan\", ";
    os << "\"frame_id\": " << snap.frameId << ", ";
    os << "\"timestamp_iso\": \"" << isoTime(snap.timestampMs) << "\", ";
    os << "\"fb_width\": "  << snap.fbWidth  << ", ";
    os << "\"fb_height\": " << snap.fbHeight << ", ";
    os << "\"command_count\": " << snap.rawCmds.size() << ", ";
    os << "\"compute_supported_types\": [\"normal_sprite\", \"scaled_sprite\", "
          "\"distorted_sprite\", \"polygon\", \"polyline\", \"line\"], ";
    os << "\"vdp1_regs\": {";
    os << "\"TVMR\": " << hexU16(snap.regs.TVMR) << ", ";
    os << "\"FBCR\": " << hexU16(snap.regs.FBCR) << ", ";
    os << "\"PTMR\": " << hexU16(snap.regs.PTMR) << ", ";
    os << "\"EWDR\": " << hexU16(snap.regs.EWDR) << ", ";
    os << "\"EWLR\": " << hexU16(snap.regs.EWLR) << ", ";
    os << "\"EWRR\": " << hexU16(snap.regs.EWRR) << ", ";
    os << "\"ENDR\": " << hexU16(snap.regs.ENDR) << ", ";
    os << "\"EDSR\": " << hexU16(snap.regs.EDSR) << ", ";
    os << "\"LOPR\": " << hexU16(snap.regs.LOPR) << ", ";
    os << "\"COPR\": " << hexU16(snap.regs.COPR) << ", ";
    os << "\"MODR\": " << hexU16(snap.regs.MODR);
    os << "}, ";
    os << "\"step_n_at_export\": " << stepNAtExport;
    os << "}";
    return os.str();
}

bool Vdp1JsonlExporter::exportSnapshot(const DebugSnapshot& snap,
                                        const std::string& path,
                                        std::string* errorMsg)
{
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        if (!f.is_open()) {
            if (errorMsg) *errorMsg = "could not open file for writing: " + path;
            return false;
        }
        f << snapshotMetaLine(snap, 0) << "\n";
        for (size_t i = 0; i < snap.rawCmds.size(); ++i) {
            f << singleCommandLine(snap, (int)i) << "\n";
        }
        f.flush();
        if (!f.good()) {
            if (errorMsg) *errorMsg = "write error";
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        if (errorMsg) *errorMsg = e.what();
        return false;
    }
}
