// Copyright 2026 devMiyax(smiyaxdev@gmail.com)
// TDD test for Vdp2Snapshot deep-copy + decodeLayers() semantics.
//
// NOTE: ASCII-only file.
#include "gtest/gtest.h"
#include "../debug/Vdp2Snapshot.h"

#include <array>
#include <cstring>
#include <vector>

namespace {

// Build an empty default Vdp2 register block. memset zeroes the unions
// safely on POD types.
Vdp2 makeRegs() {
    Vdp2 r;
    std::memset(&r, 0, sizeof(r));
    return r;
}

vdp2rotationparameter_struct makeParam() {
    vdp2rotationparameter_struct p;
    std::memset(&p, 0, sizeof(p));
    return p;
}

}  // namespace

// ---- TC-S01 ----------------------------------------------------------------
TEST(Vdp2Snapshot, TakeRawDeepCopiesVram) {
    std::vector<uint8_t> vram(512 * 1024, 0xAA);
    auto regs    = makeRegs();
    auto fixRegs = makeRegs();
    auto pa      = makeParam();
    auto pb      = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs,
                                       vram.data(), vram.size(),
                                       /*cram*/nullptr, 0,
                                       /*lineNBG0*/nullptr,
                                       /*lineNBG1*/nullptr,
                                       pa, pb,
                                       /*sw*/320, /*sh*/224, /*frameId*/42);

    EXPECT_EQ(snap.vdp2Ram.size(), 512u * 1024);
    EXPECT_EQ(snap.vdp2Ram[0], 0xAA);
    vram[0] = 0x00;
    EXPECT_EQ(snap.vdp2Ram[0], 0xAA);
}

// ---- TC-S02 ----------------------------------------------------------------
TEST(Vdp2Snapshot, TakeRawDeepCopiesCram) {
    std::vector<uint8_t> cram(4 * 1024, 0x55);
    auto regs    = makeRegs();
    auto fixRegs = makeRegs();
    auto pa      = makeParam();
    auto pb      = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs,
                                       /*vram*/nullptr, 0,
                                       cram.data(), cram.size(),
                                       nullptr, nullptr,
                                       pa, pb,
                                       320, 224, 0);

    EXPECT_EQ(snap.cram.size(), 4u * 1024);
    EXPECT_EQ(snap.cram[0], 0x55);
    cram[0] = 0x00;
    EXPECT_EQ(snap.cram[0], 0x55);
}

// ---- TC-S03 ----------------------------------------------------------------
TEST(Vdp2Snapshot, TakeRawStoresRegsAndFixRegsByValue) {
    auto regs    = makeRegs();
    auto fixRegs = makeRegs();
    regs.TVMD    = 0xABCD;
    regs.BGON    = 0x000F;
    fixRegs.TVMD = 0x1234;
    auto pa      = makeParam();
    auto pb      = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs,
                                       nullptr, 0, nullptr, 0,
                                       nullptr, nullptr,
                                       pa, pb,
                                       320, 224, 0);

    EXPECT_EQ(snap.regs.TVMD,    0xABCD);
    EXPECT_EQ(snap.regs.BGON,    0x000F);
    EXPECT_EQ(snap.fixRegs.TVMD, 0x1234);

    regs.TVMD = 0;
    EXPECT_EQ(snap.regs.TVMD, 0xABCD);
}

// ---- TC-S04 ----------------------------------------------------------------
TEST(Vdp2Snapshot, TakeRawCopiesLineTables) {
    std::array<vdp2Lineinfo, 512> ln0{};
    std::array<vdp2Lineinfo, 512> ln1{};
    ln0[10].LineScrollValH = 0x1234;
    ln1[20].LineScrollValV = 0x5678;

    auto regs    = makeRegs();
    auto fixRegs = makeRegs();
    auto pa      = makeParam();
    auto pb      = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs,
                                       nullptr, 0, nullptr, 0,
                                       ln0.data(), ln1.data(),
                                       pa, pb,
                                       320, 224, 0);

    EXPECT_EQ(snap.lineNBG0[10].LineScrollValH, 0x1234);
    EXPECT_EQ(snap.lineNBG1[20].LineScrollValV, 0x5678);
    ln0[10].LineScrollValH = 0;
    EXPECT_EQ(snap.lineNBG0[10].LineScrollValH, 0x1234);
}

// ---- TC-S05 ----------------------------------------------------------------
TEST(Vdp2Snapshot, TakeRawCopiesRotationParameters) {
    auto pa = makeParam();
    auto pb = makeParam();
    pa.Xst  = 12.5f;
    pa.A    = 2.0f;
    pb.kx   = -1.0f;

    auto regs    = makeRegs();
    auto fixRegs = makeRegs();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs,
                                       nullptr, 0, nullptr, 0,
                                       nullptr, nullptr,
                                       pa, pb,
                                       320, 224, 0);

    EXPECT_FLOAT_EQ(snap.paraA.Xst, 12.5f);
    EXPECT_FLOAT_EQ(snap.paraA.A,   2.0f);
    EXPECT_FLOAT_EQ(snap.paraB.kx, -1.0f);
}

// ---- TC-S06 ----------------------------------------------------------------
TEST(Vdp2Snapshot, TakeRawSetsMetaFields) {
    auto regs    = makeRegs();
    auto fixRegs = makeRegs();
    auto pa      = makeParam();
    auto pb      = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs,
                                       nullptr, 0, nullptr, 0,
                                       nullptr, nullptr,
                                       pa, pb,
                                       /*sw*/704, /*sh*/480, /*fid*/12345);

    EXPECT_EQ(snap.frameId,      12345u);
    EXPECT_EQ(snap.screenWidth,  704);
    EXPECT_EQ(snap.screenHeight, 480);
    EXPECT_GT(snap.timestampMs,  0u);
}

// ---- TC-S07 ----------------------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersBgonEnabled) {
    auto regs = makeRegs();
    regs.BGON = 0x0035;   // bits 0, 2, 4, 5 -> NBG0, NBG2, RBG0, RBG1
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_TRUE (snap.layers[0].enabled);   // NBG0
    EXPECT_FALSE(snap.layers[1].enabled);   // NBG1
    EXPECT_TRUE (snap.layers[2].enabled);   // NBG2
    EXPECT_FALSE(snap.layers[3].enabled);   // NBG3
    EXPECT_TRUE (snap.layers[4].enabled);   // RBG0
    EXPECT_TRUE (snap.layers[5].enabled);   // RBG1
}

// ---- TC-S08 ----------------------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersPriorityPRINA) {
    auto regs = makeRegs();
    regs.PRINA = 0x0503;   // NBG0=3, NBG1=5
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_EQ(snap.layers[0].priority, 3);
    EXPECT_EQ(snap.layers[1].priority, 5);
}

// ---- TC-S09 ----------------------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersPriorityPRINB) {
    auto regs = makeRegs();
    regs.PRINB = 0x0102;   // NBG2=2, NBG3=1
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_EQ(snap.layers[2].priority, 2);
    EXPECT_EQ(snap.layers[3].priority, 1);
}

// ---- TC-S10 ----------------------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersPriorityPRIR) {
    auto regs = makeRegs();
    regs.PRIR = 0x0006;
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_EQ(snap.layers[4].priority, 6);
}

// ---- TC-S11 ----------------------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersChctlaColorModeNBG0) {
    auto regs = makeRegs();
    // bits 4-6 = NBG0 color mode = 2 (16bpp RGB 5-5-5) -> 0x20
    regs.CHCTLA = 0x0020;
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_EQ(snap.layers[0].colorMode, 2);
}

// ---- TC-S12 ----------------------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersChctlaNBG0Bitmap) {
    auto regs = makeRegs();
    regs.CHCTLA = 0x0002;   // bit 1 = NBG0 bitmap
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_TRUE(snap.layers[0].isBitmap);
    EXPECT_FALSE(snap.layers[1].isBitmap);   // NBG1 unchanged
}

// ---- TC-S13 ----------------------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersCcctlColorCalcEnable) {
    auto regs = makeRegs();
    regs.CCCTL = 0x0011;   // NBG0 (bit0) + RBG0 (bit4)
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_TRUE (snap.layers[0].colorCalcEnabled);
    EXPECT_FALSE(snap.layers[1].colorCalcEnabled);
    EXPECT_FALSE(snap.layers[2].colorCalcEnabled);
    EXPECT_FALSE(snap.layers[3].colorCalcEnabled);
    EXPECT_TRUE (snap.layers[4].colorCalcEnabled);
}

// ---- TC-S14 ----------------------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersRotationFlag) {
    auto regs    = makeRegs();
    auto fixRegs = makeRegs();
    auto pa      = makeParam();
    auto pb      = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_FALSE(snap.layers[0].isRotation);   // NBG0
    EXPECT_FALSE(snap.layers[1].isRotation);   // NBG1
    EXPECT_FALSE(snap.layers[2].isRotation);   // NBG2
    EXPECT_FALSE(snap.layers[3].isRotation);   // NBG3
    EXPECT_TRUE (snap.layers[4].isRotation);   // RBG0
    EXPECT_TRUE (snap.layers[5].isRotation);   // RBG1
}

// ---- TC-S15 ----------------------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersNames) {
    auto regs    = makeRegs();
    auto fixRegs = makeRegs();
    auto pa      = makeParam();
    auto pb      = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_STREQ(snap.layers[0].name, "NBG0");
    EXPECT_STREQ(snap.layers[1].name, "NBG1");
    EXPECT_STREQ(snap.layers[2].name, "NBG2");
    EXPECT_STREQ(snap.layers[3].name, "NBG3");
    EXPECT_STREQ(snap.layers[4].name, "RBG0");
    EXPECT_STREQ(snap.layers[5].name, "RBG1");
}

// ---- TC-S16: PLSZ plane size --------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersPlaneSize) {
    auto regs = makeRegs();
    // NBG0=1 (2x1), NBG1=2 (2x2), NBG2=0 (1x1), NBG3=2 (2x2)
    // bits (LSB first): 0-1=01, 2-3=10, 4-5=00, 6-7=10
    // byte (MSB first): 1000 1001 = 0x89
    regs.PLSZ = 0x0089;
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_EQ(snap.layers[0].planeSize, 1);
    EXPECT_EQ(snap.layers[1].planeSize, 2);
    EXPECT_EQ(snap.layers[2].planeSize, 0);
    EXPECT_EQ(snap.layers[3].planeSize, 2);
}

// ---- TC-S17: scroll registers -------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersScroll) {
    auto regs = makeRegs();
    regs.SCXIN0 = 0x0100;   // NBG0 X = 256
    regs.SCYIN0 = 0x0040;   // NBG0 Y = 64
    regs.SCXIN1 = 0x0200;   // NBG1 X = 512
    regs.SCYIN1 = 0x0080;   // NBG1 Y = 128
    regs.SCXN2  = 0x0008;
    regs.SCYN2  = 0x0010;
    regs.SCXN3  = 0x0020;
    regs.SCYN3  = 0x0040;
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_EQ(snap.layers[0].scrollX, 0x100);
    EXPECT_EQ(snap.layers[0].scrollY, 0x040);
    EXPECT_EQ(snap.layers[1].scrollX, 0x200);
    EXPECT_EQ(snap.layers[1].scrollY, 0x080);
    EXPECT_EQ(snap.layers[2].scrollX, 0x008);
    EXPECT_EQ(snap.layers[2].scrollY, 0x010);
    EXPECT_EQ(snap.layers[3].scrollX, 0x020);
    EXPECT_EQ(snap.layers[3].scrollY, 0x040);
}

// ---- TC-S18: zoom -------------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersZoom) {
    auto regs = makeRegs();
    // ZMXN0.all & 0x7FF00 -> magnitude. For 1.0x zoom, mag = 0x10000.
    regs.ZMXN0.all = 0x10000;   // 1.0x
    regs.ZMYN0.all = 0x20000;   // 0.5x (denom=2 -> zoom = 65536/131072)
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_FLOAT_EQ(snap.layers[0].zoomX, 1.0f);
    EXPECT_FLOAT_EQ(snap.layers[0].zoomY, 0.5f);
    // NBG2/3/RBG default to 1.0.
    EXPECT_FLOAT_EQ(snap.layers[2].zoomX, 1.0f);
    EXPECT_FLOAT_EQ(snap.layers[2].zoomY, 1.0f);
}

// ---- TC-S19: mosaic -----------------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersMosaic) {
    auto regs = makeRegs();
    regs.MZCTL = 0x0015;   // bits 0, 2, 4 -> NBG0, NBG2, RBG0
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_TRUE (snap.layers[0].mosaicEnabled);
    EXPECT_FALSE(snap.layers[1].mosaicEnabled);
    EXPECT_TRUE (snap.layers[2].mosaicEnabled);
    EXPECT_FALSE(snap.layers[3].mosaicEnabled);
    EXPECT_TRUE (snap.layers[4].mosaicEnabled);
}

// ---- TC-S20: color calc ratio -------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersColorCalcRatio) {
    auto regs = makeRegs();
    regs.CCRNA = 0x0A05;   // NBG0 = 5, NBG1 = 10
    regs.CCRNB = 0x100F;   // NBG2 = 15, NBG3 = 16
    regs.CCRR  = 0x001F;   // RBG0 = 31
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_EQ(snap.layers[0].colorCalcRatio,  5);
    EXPECT_EQ(snap.layers[1].colorCalcRatio, 10);
    EXPECT_EQ(snap.layers[2].colorCalcRatio, 15);
    EXPECT_EQ(snap.layers[3].colorCalcRatio, 16);
    EXPECT_EQ(snap.layers[4].colorCalcRatio, 31);
}

// ---- TC-S21: color offset -----------------------------------------------
TEST(Vdp2Snapshot, DecodeLayersColorOffset) {
    auto regs = makeRegs();
    regs.CLOFEN = 0x0011;   // NBG0 + RBG0 enabled
    regs.CLOFSL = 0x0010;   // RBG0 -> set B, NBG0 -> set A
    regs.COAR   = 0x000A;   // +10
    regs.COAG   = 0x0014;   // +20
    regs.COAB   = 0x001E;   // +30
    regs.COBR   = 0x01F6;   // -10 (sign-ext 0x1F6 = 0b1_1111_0110 -> -10)
    regs.COBG   = 0x01EC;   // -20
    regs.COBB   = 0x01E2;   // -30
    auto fixRegs = makeRegs();
    auto pa = makeParam();
    auto pb = makeParam();

    auto snap = Vdp2Snapshot::takeRaw(regs, fixRegs, nullptr, 0, nullptr, 0,
                                       nullptr, nullptr, pa, pb, 320, 224, 0);
    snap.decodeLayers();

    EXPECT_EQ(snap.layers[0].colorOffsetSel, 0);   // set A
    EXPECT_EQ(snap.layers[0].colorOffsetR,  10);
    EXPECT_EQ(snap.layers[0].colorOffsetG,  20);
    EXPECT_EQ(snap.layers[0].colorOffsetB,  30);
    EXPECT_EQ(snap.layers[1].colorOffsetSel, -1);   // disabled
    EXPECT_EQ(snap.layers[4].colorOffsetSel, 1);   // set B
    EXPECT_EQ(snap.layers[4].colorOffsetR, -10);
    EXPECT_EQ(snap.layers[4].colorOffsetG, -20);
    EXPECT_EQ(snap.layers[4].colorOffsetB, -30);
}
