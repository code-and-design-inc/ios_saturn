// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
// TDD test for Vdp1JsonlExporter::singleCommandLine.
// ASCII-only.
#include "gtest/gtest.h"
#include "../debug/Vdp1JsonlExporter.h"

#include <filesystem>
#include <fstream>

namespace {
DebugSnapshot makeMinimalSnapshot() {
    DebugSnapshot s;
    s.frameId = 1;
    s.timestampMs = 0;
    s.fbWidth = 704;
    s.fbHeight = 512;
    s.vram.assign(512 * 1024, 0);
    s.cram.assign(4 * 1024, 0);
    return s;
}
}  // namespace

TEST(JsonlExporter, SingleCommandLineEmptyIndex) {
    auto snap = makeMinimalSnapshot();
    auto out = Vdp1JsonlExporter::singleCommandLine(snap, 0);
    EXPECT_EQ(out, "");  // index out of range -> empty string
}

TEST(JsonlExporter, SingleCommandLinePolygonContainsType) {
    auto snap = makeMinimalSnapshot();
    Vdp1Cmd c{};
    c.cmdType = VDP1C_TYPE_POLYGON;
    c.color   = 0x7C00;
    c.pmod    = 0x00C0;
    c.v0 = glm::ivec2(10, 20);
    c.v1 = glm::ivec2(50, 30);
    c.v2 = glm::ivec2(60, 80);
    c.v3 = glm::ivec2(15, 70);
    c.bbox = glm::ivec4(10, 20, 60, 80);
    snap.cmds.push_back(c);

    vdp1cmd_struct rc{};
    rc.CMDCTRL = 0x0004;  // type 4 = Polygon
    rc.CMDPMOD = 0x00C0;
    rc.CMDCOLR = 0x7C00;
    rc.CMDXA = 10; rc.CMDYA = 20;
    rc.CMDXB = 50; rc.CMDYB = 30;
    rc.CMDXC = 60; rc.CMDYC = 80;
    rc.CMDXD = 15; rc.CMDYD = 70;
    snap.rawCmds.push_back(rc);
    snap.rawToCmdIndex = {0};

    auto out = Vdp1JsonlExporter::singleCommandLine(snap, 0);
    EXPECT_NE(out.find("\"type\": \"command\""),         std::string::npos);
    EXPECT_NE(out.find("\"index\": 0"),                  std::string::npos);
    EXPECT_NE(out.find("\"command_type\": \"polygon\""), std::string::npos);
    EXPECT_NE(out.find("\"CMDCTRL\": \"0x0004\""),       std::string::npos);
    EXPECT_NE(out.find("\"v0\": [10, 20]"),              std::string::npos);
    EXPECT_NE(out.find("\"bbox\": [10, 20, 60, 80]"),    std::string::npos);
    EXPECT_NE(out.find("\"status\": \"drawn\""),         std::string::npos);
    // No newline (JSONL one-liner).
    EXPECT_EQ(out.find('\n'), std::string::npos);
}

TEST(JsonlExporter, SingleCommandLineDistortedSpriteHasComputeBlock) {
    auto snap = makeMinimalSnapshot();
    Vdp1Cmd c{};
    c.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    c.color   = 0x80;
    c.pmod    = 0x008C;  // 4 bpp LUT (color mode bits 5-3 = 001)
    c.srca    = 0x10;
    c.charSize = glm::uvec2(64, 8);
    c.v0 = glm::ivec2(362, 276);
    c.v1 = glm::ivec2(426, 276);
    c.v2 = glm::ivec2(426, 340);
    c.v3 = glm::ivec2(362, 340);
    c.bbox = glm::ivec4(362, 276, 426, 340);
    snap.cmds.push_back(c);

    vdp1cmd_struct rc{};
    rc.CMDCTRL = 0x0083;
    rc.CMDPMOD = 0x008C;  // 4 bpp LUT (color mode bits 5-3 = 001)
    rc.CMDCOLR = 0x0080;
    rc.CMDSRCA = 0x0010;
    rc.CMDSIZE = 0x0808;
    rc.CMDXA = 10; rc.CMDYA = 20;
    rc.CMDXB = 74; rc.CMDYB = 20;
    rc.CMDXC = 74; rc.CMDYC = 84;
    rc.CMDXD = 10; rc.CMDYD = 84;
    snap.rawCmds.push_back(rc);
    snap.rawToCmdIndex = {0};

    auto out = Vdp1JsonlExporter::singleCommandLine(snap, 0);
    EXPECT_NE(out.find("\"command_type\": \"distorted_sprite\""), std::string::npos);
    EXPECT_NE(out.find("\"compute\": {"),                        std::string::npos);
    EXPECT_NE(out.find("\"v0\": [362, 276]"),                    std::string::npos);
    EXPECT_NE(out.find("\"color_mode\": \"4bpp_lut\""),          std::string::npos);
    EXPECT_NE(out.find("\"texture_size\": [64, 8]"),             std::string::npos);
}

TEST(JsonlExporter, SingleCommandLineUnsupportedTypeShowsSkipped) {
    auto snap = makeMinimalSnapshot();
    // Polyline: not compute-drawable, so cmds stays empty and rawToCmdIndex
    // points to -1.
    vdp1cmd_struct rc{};
    rc.CMDCTRL = 0x0005;  // Polyline (not supported by compute)
    snap.rawCmds.push_back(rc);
    snap.rawToCmdIndex = {-1};

    auto out = Vdp1JsonlExporter::singleCommandLine(snap, 0);
    EXPECT_NE(out.find("\"command_type\": \"polyline\""),                std::string::npos);
    EXPECT_NE(out.find("\"status\": \"skipped_unsupported\""),           std::string::npos);
    EXPECT_NE(out.find("\"compute\": null"),                             std::string::npos);
}

TEST(JsonlExporter, SnapshotMetaLineHasRequiredFields) {
    auto snap = makeMinimalSnapshot();
    snap.frameId = 42;
    snap.regs.TVMR = 0x1234;
    snap.regs.FBCR = 0x5678;

    auto out = Vdp1JsonlExporter::snapshotMetaLine(snap, 5);
    EXPECT_NE(out.find("\"type\": \"snapshot_meta\""),    std::string::npos);
    EXPECT_NE(out.find("\"schema_version\": 1"),          std::string::npos);
    EXPECT_NE(out.find("\"frame_id\": 42"),               std::string::npos);
    EXPECT_NE(out.find("\"fb_width\": 704"),              std::string::npos);
    EXPECT_NE(out.find("\"fb_height\": 512"),             std::string::npos);
    EXPECT_NE(out.find("\"command_count\": 0"),           std::string::npos);
    EXPECT_NE(out.find("\"step_n_at_export\": 5"),        std::string::npos);
    EXPECT_NE(out.find("\"TVMR\": \"0x1234\""),           std::string::npos);
    EXPECT_NE(out.find("\"FBCR\": \"0x5678\""),           std::string::npos);
    EXPECT_EQ(out.find('\n'), std::string::npos);
}

TEST(JsonlExporter, ExportSnapshotWritesFile) {
    auto snap = makeMinimalSnapshot();
    snap.frameId = 7;

    Vdp1Cmd c{};
    c.cmdType = VDP1C_TYPE_POLYGON;
    snap.cmds.push_back(c);
    vdp1cmd_struct rc{};
    rc.CMDCTRL = 0x0004;
    snap.rawCmds.push_back(rc);
    snap.rawToCmdIndex = {0};

    auto path = std::filesystem::temp_directory_path() / "vdp1_jsonl_export_test.jsonl";
    std::filesystem::remove(path);

    std::string err;
    bool ok = Vdp1JsonlExporter::exportSnapshot(snap, path.string(), &err);
    ASSERT_TRUE(ok) << "err=" << err;
    ASSERT_TRUE(std::filesystem::exists(path));

    std::ifstream f(path);
    std::string line1, line2, line3;
    std::getline(f, line1);
    std::getline(f, line2);
    std::getline(f, line3);
    EXPECT_NE(line1.find("\"type\": \"snapshot_meta\""), std::string::npos);
    EXPECT_NE(line2.find("\"type\": \"command\""),      std::string::npos);
    EXPECT_TRUE(line3.empty());  // 2 lines only

    std::filesystem::remove(path);
}

TEST(JsonlExporter, RawIndexAndComputeIndexDivergeUsesRawToCmdIndex) {
    // Scenario: rawCmds has 3 entries but only 2 are compute-drawable.
    //   raw[0] = Polygon            -> compute idx 0
    //   raw[1] = Normal Sprite      -> NOT compute (idx -1)
    //   raw[2] = Distorted Sprite   -> compute idx 1
    //
    // singleCommandLine(snap, 2) must dereference snap.cmds[ rawToCmdIndex[2] ]
    // = snap.cmds[1] (the distorted sprite). The previous implementation used
    // snap.cmds[2] directly, which is out of range (cmds.size()==2) and silently
    // returned a default Vdp1Cmd{} (cmdType=NOOP), producing a misleading
    // "compute": null line for what is actually a compute-drawn distorted sprite.
    auto snap = makeMinimalSnapshot();
    Vdp1Cmd polygon{};
    polygon.cmdType = VDP1C_TYPE_POLYGON;
    polygon.color   = 0x1111;
    polygon.v0 = glm::ivec2(1, 1);
    polygon.v1 = glm::ivec2(2, 1);
    polygon.v2 = glm::ivec2(2, 2);
    polygon.v3 = glm::ivec2(1, 2);
    polygon.bbox = glm::ivec4(1, 1, 2, 2);
    snap.cmds.push_back(polygon);

    Vdp1Cmd distorted{};
    distorted.cmdType  = VDP1C_TYPE_DISTORTED_SPRITE;
    distorted.color    = 0x2222;
    distorted.srca     = 0x10;
    distorted.charSize = glm::uvec2(96, 80);  // distinctive size
    distorted.pmod     = 0x008C;              // 4bpp LUT
    distorted.v0 = glm::ivec2(900, 900);
    distorted.v1 = glm::ivec2(901, 900);
    distorted.v2 = glm::ivec2(901, 901);
    distorted.v3 = glm::ivec2(900, 901);
    distorted.bbox = glm::ivec4(900, 900, 901, 901);
    snap.cmds.push_back(distorted);

    vdp1cmd_struct rc0{};
    rc0.CMDCTRL = 0x0004;  // Polygon
    snap.rawCmds.push_back(rc0);

    vdp1cmd_struct rc1{};
    rc1.CMDCTRL = 0x0000;  // Normal Sprite (not compute)
    snap.rawCmds.push_back(rc1);

    vdp1cmd_struct rc2{};
    rc2.CMDCTRL = 0x0002;  // Distorted Sprite
    rc2.CMDPMOD = 0x008C;
    rc2.CMDCOLR = 0x2222;
    rc2.CMDSRCA = 0x0010;
    rc2.CMDSIZE = 0x0C50;  // (0x0C * 8, 0x50) = (96, 80) - matches distorted.charSize
    snap.rawCmds.push_back(rc2);

    snap.rawToCmdIndex = {0, -1, 1};

    auto out = Vdp1JsonlExporter::singleCommandLine(snap, 2);

    EXPECT_NE(out.find("\"command_type\": \"distorted_sprite\""), std::string::npos);
    // Must reference cmds[1]'s data, not cmds[2] (out of range).
    EXPECT_NE(out.find("\"compute\": {"), std::string::npos)
        << "expected 'compute' block, got: " << out;
    EXPECT_NE(out.find("\"v0\": [900, 900]"), std::string::npos)
        << "expected v0 from distorted (cmds[1]), got: " << out;
    EXPECT_NE(out.find("\"char_size_pixels\": [96, 80]"), std::string::npos)
        << "expected charSize from distorted (cmds[1]), got: " << out;
    EXPECT_NE(out.find("\"status\": \"drawn\""), std::string::npos);
}

TEST(JsonlExporter, NormalSpriteNullsUnusedRawCoords) {
    // A normal_sprite (type 0) only uses CMDXA/CMDYA. The remaining
    // coordinate words (B/C/D) are not part of the command and hold
    // whatever stale bytes the game left in VRAM. They must be emitted as
    // null so the raw block does not present garbage as "coordinates".
    auto snap = makeMinimalSnapshot();
    vdp1cmd_struct rc{};
    rc.CMDCTRL = 0x0000;  // normal sprite
    rc.CMDXA = 43;  rc.CMDYA = 162;
    rc.CMDXB = 0;   rc.CMDYB = 12;
    rc.CMDXC = 1546; rc.CMDYC = 6488;   // garbage
    rc.CMDXD = 1551; rc.CMDYD = 26188;  // garbage
    snap.rawCmds.push_back(rc);
    snap.rawToCmdIndex = {-1};

    auto out = Vdp1JsonlExporter::singleCommandLine(snap, 0);
    EXPECT_NE(out.find("\"CMDXA\": 43"),    std::string::npos);
    EXPECT_NE(out.find("\"CMDYA\": 162"),   std::string::npos);
    EXPECT_NE(out.find("\"CMDXB\": null"),  std::string::npos);
    EXPECT_NE(out.find("\"CMDYB\": null"),  std::string::npos);
    EXPECT_NE(out.find("\"CMDXC\": null"),  std::string::npos);
    EXPECT_NE(out.find("\"CMDYC\": null"),  std::string::npos);
    EXPECT_NE(out.find("\"CMDXD\": null"),  std::string::npos);
    EXPECT_NE(out.find("\"CMDYD\": null"),  std::string::npos);
    // The garbage must not leak through as a number.
    EXPECT_EQ(out.find("6488"),  std::string::npos);
    EXPECT_EQ(out.find("26188"), std::string::npos);
}

TEST(JsonlExporter, ScaledSpriteNullsUnusedRawCoordsByZoomPoint) {
    // Scaled sprite (type 1) with zoom point != 0 uses A + B (size in B);
    // C/D are unused. With zoom point 0 it uses A + C; B/D are unused.
    {
        auto snap = makeMinimalSnapshot();
        vdp1cmd_struct rc{};
        rc.CMDCTRL = 0x0501;  // scaled, zoom point (bits 8-11) = 5 -> uses B
        rc.CMDXA = 10; rc.CMDYA = 20;
        rc.CMDXB = 40; rc.CMDYB = 30;
        rc.CMDXC = 999; rc.CMDYC = 888;  // garbage
        rc.CMDXD = 777; rc.CMDYD = 666;  // garbage
        snap.rawCmds.push_back(rc);
        snap.rawToCmdIndex = {-1};
        auto out = Vdp1JsonlExporter::singleCommandLine(snap, 0);
        EXPECT_NE(out.find("\"CMDXB\": 40"),   std::string::npos);
        EXPECT_NE(out.find("\"CMDXC\": null"), std::string::npos);
        EXPECT_NE(out.find("\"CMDXD\": null"), std::string::npos);
    }
    {
        auto snap = makeMinimalSnapshot();
        vdp1cmd_struct rc{};
        rc.CMDCTRL = 0x0001;  // scaled, zoom point 0 -> uses C
        rc.CMDXA = 10; rc.CMDYA = 20;
        rc.CMDXB = 999; rc.CMDYB = 888;  // garbage
        rc.CMDXC = 60;  rc.CMDYC = 80;
        rc.CMDXD = 777; rc.CMDYD = 666;  // garbage
        snap.rawCmds.push_back(rc);
        snap.rawToCmdIndex = {-1};
        auto out = Vdp1JsonlExporter::singleCommandLine(snap, 0);
        EXPECT_NE(out.find("\"CMDXB\": null"), std::string::npos);
        EXPECT_NE(out.find("\"CMDXC\": 60"),   std::string::npos);
        EXPECT_NE(out.find("\"CMDXD\": null"), std::string::npos);
    }
}

TEST(JsonlExporter, ExportSnapshotFailsForInvalidPath) {
    auto snap = makeMinimalSnapshot();
    std::string err;
    // Use the temp directory itself as the target path. ofstream cannot open
    // a directory for writing, so this exercises the open-failure path on
    // both Windows and POSIX. (The spec example "/this/path/does/not/exist"
    // would be auto-created by create_directories on Windows, defeating the
    // test intent.)
    auto dirPath = std::filesystem::temp_directory_path();
    bool ok = Vdp1JsonlExporter::exportSnapshot(
        snap, dirPath.string(), &err);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(err.empty());
}
