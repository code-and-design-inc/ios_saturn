// Copyright 2026 devMiyax
//
// VDP2 compositor test harness (issue #22, task T-002).
//
// This file establishes the gtest harness for the new shader-based VDP2 color
// calculation pipeline. It compiles against the host-port reference oracle
// (vdp2_color_oracle.h, task T-001) the same way vdp1_compute_sort_test.cpp is
// a standalone gtest target: no emulator/vulkan port linkage, just the oracle
// header plus googletest (FetchContent).
//
// Right now this only contains smoke tests that prove the harness builds, the
// oracle includes cleanly, and the helper fixture can call into it. Tasks
// T-005 / T-006 (and later compositor unit tests UT-001..UT-014) add their real
// cases to this same target. See the "for T-005/T-006" notes below.
//
// ASCII-only comments (CLAUDE.md rule: MSVC CP932 -> C4819/C2065).

#include "gtest/gtest.h"

#include "vdp2_color_oracle.h"
#include "Vdp2ColorCalcState.h"  // T-005 register decoder (../Vdp2ColorCalcState.h)
#include "vdp2_sprite_decode.h"  // T-009 sprite decode reference

#include <array>
#include <cstdint>
#include <ios>

namespace {

using namespace vdp2oracle;

// ---------------------------------------------------------------------------
// Shared fixture. Provides small helpers so future per-pixel / per-register
// cases (T-005 register decode, T-006 attr+priority) do not each re-roll CRAM
// setup or layer-array construction. Keep helpers thin and oracle-faithful.
// ---------------------------------------------------------------------------
class Vdp2CompositorTest : public ::testing::Test {
 protected:
    // Build a CRAM image (word entries) from a list of RGB555 values so a test
    // can ask cramGetColor() for a known index. `cram` is sized to the 4 KiB
    // address space the oracle masks against (addr & 0xFFF).
    std::array<uint8_t, 0x1000> cram{};

    // Write a 16-bit RGB555 value at CRAM word index `idx` (little-endian, the
    // byte order readWordT2() expects for the default desktop build).
    void setCramWord(uint32_t idx, uint16_t rgb555) {
        uint32_t byteAddr = (idx << 1) & 0xFFF;
        cram[byteAddr] = static_cast<uint8_t>(rgb555 & 0xFF);
        cram[byteAddr + 1] = static_cast<uint8_t>((rgb555 >> 8) & 0xFF);
    }

    // Write a 32-bit RGB888 value at CRAM long index `idx` (color mode 2).
    void setCramLong(uint32_t idx, uint32_t rgb888) {
        uint32_t byteAddr = (idx << 2) & 0xFFF;
        cram[byteAddr] = static_cast<uint8_t>(rgb888 & 0xFF);
        cram[byteAddr + 1] = static_cast<uint8_t>((rgb888 >> 8) & 0xFF);
        cram[byteAddr + 2] = static_cast<uint8_t>((rgb888 >> 16) & 0xFF);
        cram[byteAddr + 3] = static_cast<uint8_t>((rgb888 >> 24) & 0xFF);
    }

    // Construct an empty (all-transparent) layer array for selectTopTwo /
    // composite cases. Tests fill in the slots they care about.
    static std::array<LayerPixel, kLayerCount> emptyLayers() {
        return std::array<LayerPixel, kLayerCount>{};
    }
};

// ---------------------------------------------------------------------------
// Smoke tests: prove the oracle is reachable and behaves on known inputs.
// These are NOT the real parity suite; they only validate the harness wiring.
// ---------------------------------------------------------------------------

// attr pack/unpack round-trip (precursor to UT-003, owned by T-006).
TEST_F(Vdp2CompositorTest, AttrPackUnpackRoundTrip) {
    Attr a;
    a.priority = 23;       // fits in 5 bits
    a.ccEnable = true;
    a.ccRatio = 0x2A;      // fits in 6 bits
    a.transparent = true;

    uint32_t packed = packAttr(a);
    Attr b = unpackAttr(packed);

    EXPECT_EQ(b.priority, a.priority);
    EXPECT_EQ(b.ccEnable, a.ccEnable);
    EXPECT_EQ(b.ccRatio, a.ccRatio);
    EXPECT_EQ(b.transparent, a.transparent);
}

// attr field isolation: each field lands in its documented bit position.
TEST_F(Vdp2CompositorTest, AttrFieldBitPositions) {
    Attr a;
    a.priority = 0x1F;
    EXPECT_EQ(packAttr(a) & 0x1F, 0x1Fu);

    a = Attr{};
    a.ccEnable = true;
    EXPECT_EQ(packAttr(a), (1u << 5));

    a = Attr{};
    a.ccRatio = 0x3F;
    EXPECT_EQ(packAttr(a), (0x3Fu << 6));

    a = Attr{};
    a.transparent = true;
    EXPECT_EQ(packAttr(a), (1u << 12));
}

// CRAM word fetch (mode 0/1): RGB555 -> 0x00RRGGBB with the documented MSB
// preserved at bit 31. White (0x7FFF) expands to 0xF8 per channel.
TEST_F(Vdp2CompositorTest, CramGetColorWordKnownValues) {
    setCramWord(/*idx*/ 5, 0x7FFF);  // RGB555 white, MSB clear
    uint32_t white = cramGetColor(/*colorMode*/ 0, /*addr*/ 5, cram.data());
    EXPECT_EQ(getRed(white), 0xF8);
    EXPECT_EQ(getGreen(white), 0xF8);
    EXPECT_EQ(getBlue(white), 0xF8);
    EXPECT_EQ(white & 0x80000000u, 0u);

    setCramWord(/*idx*/ 6, 0x8000);  // MSB set, color 0
    uint32_t msb = cramGetColor(/*colorMode*/ 1, /*addr*/ 6, cram.data());
    EXPECT_EQ(msb & 0x00FFFFFFu, 0u);
    EXPECT_EQ(msb & 0x80000000u, 0x80000000u);

    // RGB555 bits 0-4 are BLUE (the oracle maps (tmp & 0x1F) << 3 to the low
    // byte). bits 10-14 are red; see cramGetColor() in vdp2_color_oracle.h.
    setCramWord(/*idx*/ 7, 0x001F);  // pure blue in RGB555 (bits 0-4)
    uint32_t blue = cramGetColor(/*colorMode*/ 0, /*addr*/ 7, cram.data());
    EXPECT_EQ(getRed(blue), 0x00);
    EXPECT_EQ(getGreen(blue), 0x00);
    EXPECT_EQ(getBlue(blue), 0xF8);

    setCramWord(/*idx*/ 8, 0x7C00);  // pure red in RGB555 (bits 10-14)
    uint32_t red = cramGetColor(/*colorMode*/ 0, /*addr*/ 8, cram.data());
    EXPECT_EQ(getRed(red), 0xF8);
    EXPECT_EQ(getGreen(red), 0x00);
    EXPECT_EQ(getBlue(red), 0x00);
}

// CRAM long fetch (mode 2): RGB888 returned verbatim (masked to 0xFFF addr).
TEST_F(Vdp2CompositorTest, CramGetColorLongKnownValue) {
    setCramLong(/*idx*/ 3, 0x00123456u);
    uint32_t c = cramGetColor(/*colorMode*/ 2, /*addr*/ 3, cram.data());
    EXPECT_EQ(getRed(c), 0x12);
    EXPECT_EQ(getGreen(c), 0x34);
    EXPECT_EQ(getBlue(c), 0x56);
}

// Priority select tiebreak: higher LayerId wins within the same priority
// (precursor to UT-004, owned by T-006).
TEST_F(Vdp2CompositorTest, SelectTopTwoTiebreakByLayerId) {
    auto layers = emptyLayers();
    layers[kNBG0].pixel = 0x00AA0000u;
    layers[kNBG0].priority = 4;
    layers[kSprite].pixel = 0x000000BBu;
    layers[kSprite].priority = 4;  // same priority as NBG0

    LayerPixel back;
    back.pixel = 0x00000000u;

    TopTwo t = selectTopTwo(layers, back);
    // kSprite (id 5) outranks kNBG0 (id 3) on the tie -> stack[0] is sprite.
    EXPECT_EQ(t.stack[0].pixel, 0x000000BBu);
    EXPECT_EQ(t.stack[1].pixel, 0x00AA0000u);
    EXPECT_FALSE(t.usedBack);
}

// Back screen fills empty slots when fewer than two opaque layers exist.
TEST_F(Vdp2CompositorTest, SelectTopTwoFallsBackToBack) {
    auto layers = emptyLayers();  // all priority 0 -> transparent
    LayerPixel back;
    back.pixel = 0x00112233u;

    TopTwo t = selectTopTwo(layers, back);
    EXPECT_TRUE(t.usedBack);
    EXPECT_EQ(t.stack[0].pixel, back.pixel);
}

// Basic-pattern composite with no color calc: top layer passes through
// unchanged (precursor to UT-005, owned by T-008).
TEST_F(Vdp2CompositorTest, CompositeBasicPatternTopWins) {
    auto layers = emptyLayers();
    layers[kNBG1].pixel = createPixel(0x3F, 0x10, 0x20, 0x30);
    layers[kNBG1].priority = 6;

    LayerPixel back;
    back.pixel = createPixel(0x3F, 0, 0, 0);

    // BlendMode::Top with alpha 0x3F => transTest is false => no blend.
    uint32_t out = composite(layers, back, BlendMode::Top,
                             /*lineColorForRow*/ 0u, /*spctl*/ 0u);
    EXPECT_EQ(getRed(out), 0x10);
    EXPECT_EQ(getGreen(out), 0x20);
    EXPECT_EQ(getBlue(out), 0x30);
}

// ---------------------------------------------------------------------------
// T-006 real suite: attr pack/unpack (UT-003), priority sort/select (UT-004),
// same-priority tiebreak (UT-B03), out-of-range priority clamp (UT-E02).
//
// These pin the oracle's packAttr/unpackAttr and selectSortedLayers as the
// authoritative host-port reference that the GLSL side (T-004 packGBufferAttr
// already matches the bit layout; T-008 sort/select will match the loop) must
// reproduce 1:1.
// ---------------------------------------------------------------------------

// UT-003: attr pack -> unpack is a complete round-trip across the full valid
// range of every field. Mirrors the R32_UINT G-buffer attr encoding shared
// with GLSL packGBufferAttr() (VdpPipeline.cpp).
TEST_F(Vdp2CompositorTest, UT003_AttrPackUnpackFullRange) {
    for (int prio = 0; prio <= 0x1F; ++prio) {
        for (int ratio = 0; ratio <= 0x3F; ++ratio) {
            for (int ccEn = 0; ccEn <= 1; ++ccEn) {
                for (int trans = 0; trans <= 1; ++trans) {
                    Attr a;
                    a.priority = static_cast<uint8_t>(prio);
                    a.ccRatio = static_cast<uint8_t>(ratio);
                    a.ccEnable = (ccEn != 0);
                    a.transparent = (trans != 0);

                    Attr b = unpackAttr(packAttr(a));
                    EXPECT_EQ(b.priority, a.priority);
                    EXPECT_EQ(b.ccRatio, a.ccRatio);
                    EXPECT_EQ(b.ccEnable, a.ccEnable);
                    EXPECT_EQ(b.transparent, a.transparent);
                }
            }
        }
    }
}

// UT-003: spare bits 13..31 are never set by packAttr, and unpackAttr ignores
// them (a stray spare bit must not corrupt any field).
TEST_F(Vdp2CompositorTest, UT003_AttrSpareBitsIgnored) {
    Attr a;
    a.priority = 7;
    a.ccEnable = true;
    a.ccRatio = 0x15;
    a.transparent = true;

    uint32_t packed = packAttr(a);
    EXPECT_EQ(packed & 0xFFFFE000u, 0u);  // bits 13..31 clear

    Attr b = unpackAttr(packed | 0xFFFFE000u);  // force all spare bits set
    EXPECT_EQ(b.priority, a.priority);
    EXPECT_EQ(b.ccEnable, a.ccEnable);
    EXPECT_EQ(b.ccRatio, a.ccRatio);
    EXPECT_EQ(b.transparent, a.transparent);
}

// UT-004: six layers with distinct priorities sort descending; top..fourth
// (and beyond) come out in priority order. Verifies the full sorted-layer
// list, not just the top two.
TEST_F(Vdp2CompositorTest, UT004_PrioritySortDescending) {
    auto layers = emptyLayers();
    // Assign distinct priorities; LayerId order intentionally NOT matching
    // priority order so the sort (not the array order) is what is tested.
    layers[kNBG3].priority = 2;
    layers[kNBG2].priority = 5;
    layers[kNBG1].priority = 1;
    layers[kNBG0].priority = 7;
    layers[kRBG0].priority = 3;
    layers[kSprite].priority = 6;

    std::array<int, kLayerCount> sorted{};
    int count = 0;
    selectSortedLayers(layers, sorted, count);

    EXPECT_EQ(count, 6);
    EXPECT_EQ(sorted[0], kNBG0);    // priority 7
    EXPECT_EQ(sorted[1], kSprite);  // priority 6
    EXPECT_EQ(sorted[2], kNBG2);    // priority 5
    EXPECT_EQ(sorted[3], kRBG0);    // priority 3 -> "fourth"
    EXPECT_EQ(sorted[4], kNBG3);    // priority 2
    EXPECT_EQ(sorted[5], kNBG1);    // priority 1
}

// UT-004: transparent (priority 0) layers are excluded from the sorted list,
// and `count` reflects only the opaque layers. selectTopTwo agrees with the
// sorted-list head.
TEST_F(Vdp2CompositorTest, UT004_TransparentLayersExcluded) {
    auto layers = emptyLayers();
    layers[kNBG0].priority = 4;
    layers[kNBG0].pixel = 0x00AABBCCu;
    layers[kRBG0].priority = 6;
    layers[kRBG0].pixel = 0x00112233u;
    // others stay priority 0 (transparent).

    std::array<int, kLayerCount> sorted{};
    int count = 0;
    selectSortedLayers(layers, sorted, count);

    EXPECT_EQ(count, 2);
    EXPECT_EQ(sorted[0], kRBG0);  // priority 6
    EXPECT_EQ(sorted[1], kNBG0);  // priority 4
    EXPECT_EQ(sorted[2], kBack);  // sentinel beyond the opaque count

    LayerPixel back;
    back.pixel = 0x00000000u;
    TopTwo t = selectTopTwo(layers, back);
    EXPECT_EQ(t.stack[0].pixel, layers[kRBG0].pixel);
    EXPECT_EQ(t.stack[1].pixel, layers[kNBG0].pixel);
    EXPECT_FALSE(t.usedBack);
}

// UT-B03: when several layers share one priority, the index tiebreak orders
// them by LayerId descending (Sprite > RBG0 > NBG0 > NBG1 > NBG2 > NBG3),
// matching titan's inner scan from kSprite down to kNBG3.
TEST_F(Vdp2CompositorTest, UTB03_SamePriorityTiebreakByLayerId) {
    auto layers = emptyLayers();
    const uint8_t kSamePrio = 4;
    layers[kNBG3].priority = kSamePrio;
    layers[kNBG2].priority = kSamePrio;
    layers[kNBG1].priority = kSamePrio;
    layers[kNBG0].priority = kSamePrio;
    layers[kRBG0].priority = kSamePrio;
    layers[kSprite].priority = kSamePrio;

    std::array<int, kLayerCount> sorted{};
    int count = 0;
    selectSortedLayers(layers, sorted, count);

    EXPECT_EQ(count, 6);
    EXPECT_EQ(sorted[0], kSprite);
    EXPECT_EQ(sorted[1], kRBG0);
    EXPECT_EQ(sorted[2], kNBG0);
    EXPECT_EQ(sorted[3], kNBG1);
    EXPECT_EQ(sorted[4], kNBG2);
    EXPECT_EQ(sorted[5], kNBG3);
}

// UT-B03: mixed priorities where the top priority is shared. The shared-prio
// pair resolves by LayerId, then the lower-priority layer follows.
TEST_F(Vdp2CompositorTest, UTB03_TiebreakAtTopThenLowerPriority) {
    auto layers = emptyLayers();
    layers[kNBG0].priority = 6;   // ties with sprite at top
    layers[kSprite].priority = 6;
    layers[kNBG2].priority = 3;   // below the tie

    std::array<int, kLayerCount> sorted{};
    int count = 0;
    selectSortedLayers(layers, sorted, count);

    EXPECT_EQ(count, 3);
    EXPECT_EQ(sorted[0], kSprite);  // tiebreak: higher LayerId first
    EXPECT_EQ(sorted[1], kNBG0);
    EXPECT_EQ(sorted[2], kNBG2);
}

// UT-E02: an out-of-range priority value (> 7, storable in the 5-bit attr
// field) is clamped to kMaxPriority by clampPriority and still participates in
// the sort instead of being dropped or causing undefined behaviour.
TEST_F(Vdp2CompositorTest, UTE02_OutOfRangePriorityClamped) {
    EXPECT_EQ(clampPriority(0), 0);
    EXPECT_EQ(clampPriority(7), 7);
    EXPECT_EQ(clampPriority(8), kMaxPriority);
    EXPECT_EQ(clampPriority(0x1F), kMaxPriority);  // max 5-bit value
    EXPECT_EQ(clampPriority(-1), 0);

    auto layers = emptyLayers();
    layers[kRBG0].priority = 0x1F;  // out of range -> clamps to 7 (top)
    layers[kRBG0].pixel = 0x00DDEEFFu;
    layers[kNBG0].priority = 7;     // legitimate top; tiebreak with clamped RBG0
    layers[kNBG0].pixel = 0x00010203u;

    std::array<int, kLayerCount> sorted{};
    int count = 0;
    selectSortedLayers(layers, sorted, count);

    // Both clamp to priority 7; LayerId tiebreak: kRBG0 (4) > kNBG0 (3).
    EXPECT_EQ(count, 2);
    EXPECT_EQ(sorted[0], kRBG0);
    EXPECT_EQ(sorted[1], kNBG0);
}

// UT-E02: a packed attr carrying a priority that exceeds the 3-bit hardware
// range round-trips its raw 5-bit value through unpack, and clampPriority maps
// it into range for selection (the attr stores it, the sort clamps it).
TEST_F(Vdp2CompositorTest, UTE02_PackedOutOfRangePriorityRoundTripThenClamp) {
    Attr a;
    a.priority = 0x1F;  // 31, far above hardware max 7
    uint32_t packed = packAttr(a);
    Attr b = unpackAttr(packed);
    EXPECT_EQ(b.priority, 0x1F);                 // raw value preserved
    EXPECT_EQ(clampPriority(b.priority), kMaxPriority);  // clamped for sort
}

// ---------------------------------------------------------------------------
// T-005 real suite: Vdp2ColorCalcState register decode (UT-001, UT-002, UT-009).
//
// These pin Vdp2ColorCalcState.h (vdp2cc::decodeVdp2ColorCalc) as the
// authoritative host-port reference for the CCCTL/CCRNA/CCRNB/CCRR/CCRS/CCRTMD
// decode, cross-checked against vdp2_color_oracle.h decodeLayerAlpha /
// decodeSpriteColorCalcTable / decodeLineColorAlpha (the vidsoft baseline).
// ---------------------------------------------------------------------------

using namespace vdp2cc;

namespace {
// Convenience: build a State with all ratio regs = 0 (so ~reg & 0x1F == 0x1F,
// ratio == 0x3F) unless a test overrides them.
State decode(uint16_t cctl,
             uint16_t ccrna = 0, uint16_t ccrnb = 0, uint16_t ccrr = 0,
             std::array<uint16_t, 4> ccrs = {0, 0, 0, 0}, uint16_t ccrlb = 0) {
    return decodeVdp2ColorCalc(cctl, ccrna, ccrnb, ccrr, ccrs, ccrlb);
}
}  // namespace

// UT-001: CCCTL per-screen enable bits decode to the correct per-layer
// ccEnable. With CCRTMD (0x200) clear, the enable bit alone drives ccEnable
// (vidsoft: cctl & (0x200 | enableBit)).
TEST_F(Vdp2CompositorTest, UT001_CcctlPerLayerEnable) {
    // NBG0 only.
    {
        State s = decode(vdp2cc::kCcEnNBG0);
        EXPECT_TRUE(s.perLayer[vdp2cc::kNBG0].ccEnable);
        EXPECT_FALSE(s.perLayer[vdp2cc::kNBG1].ccEnable);
        EXPECT_FALSE(s.perLayer[vdp2cc::kNBG2].ccEnable);
        EXPECT_FALSE(s.perLayer[vdp2cc::kNBG3].ccEnable);
        EXPECT_FALSE(s.perLayer[vdp2cc::kRBG0].ccEnable);
    }
    // Each enable bit maps to its layer.
    EXPECT_TRUE(decode(vdp2cc::kCcEnNBG1).perLayer[vdp2cc::kNBG1].ccEnable);
    EXPECT_TRUE(decode(vdp2cc::kCcEnNBG2).perLayer[vdp2cc::kNBG2].ccEnable);
    EXPECT_TRUE(decode(vdp2cc::kCcEnNBG3).perLayer[vdp2cc::kNBG3].ccEnable);
    EXPECT_TRUE(decode(vdp2cc::kCcEnRBG0).perLayer[vdp2cc::kRBG0].ccEnable);
    EXPECT_TRUE(decode(vdp2cc::kCcEnSprite).perLayer[vdp2cc::kSprite].ccEnable);
}

// UT-001: CCCTL with CCRTMD (0x200) set does NOT enable color calc on layers
// whose per-screen enable bit is clear. vidsoft's "if (CCCTL & 0x201)" only
// guards the info.alpha RATIO computation (so a cc-off layer can still supply
// its ratio as the SECOND operand in ratio-from-second mode); the actual blend
// gate is titan transTest(), which reduces to the enable bit in every mode.
// The old OR decode blended every layer half-transparent in CCRTMD games
// (Space Harrier: CCCTL=0x0240 washed out NBG1/NBG2 against the back screen).
TEST_F(Vdp2CompositorTest, UT001_CcrtmdAloneDoesNotEnableLayers) {
    State s = decode(vdp2cc::kCcrtmd, /*ccrna*/ 0x1717, /*ccrnb*/ 0x1717);
    for (int i = 0; i < vdp2cc::kLayerCount; ++i) {
        EXPECT_FALSE(s.perLayer[i].ccEnable) << "layer " << i;
    }
    // ...but the ratio is still decoded (second-operand supply in BOTTOM mode).
    EXPECT_EQ(s.perLayer[vdp2cc::kNBG0].ratio,
              (((~0x1717 & 0x1F) << 1) + 1));
    EXPECT_EQ(s.perLayer[vdp2cc::kNBG2].ratio,
              (((~0x1717 & 0x1F) << 1) + 1));
}

// UT-001: CCMD bit (0x100) selects mode (ratio vs add); CCRTMD bit (0x200)
// selects ratio source (top vs second).
TEST_F(Vdp2CompositorTest, UT001_CcctlModeAndRatioSource) {
    EXPECT_EQ(decode(0).mode, vdp2cc::CalcMode::Ratio);
    EXPECT_EQ(decode(vdp2cc::kCcmd).mode, vdp2cc::CalcMode::Add);

    EXPECT_EQ(decode(0).ratioSource, vdp2cc::RatioSource::Top);
    EXPECT_EQ(decode(vdp2cc::kCcrtmd).ratioSource, vdp2cc::RatioSource::Second);
}

// UT-001: the 0x80 trans flag follows vidsoft exactly:
//   (CCCTL & (0x200|enable)) == (0x200|enable)  -> trans
//   (CCCTL & (0x100|enable)) == (0x100|enable)  -> trans
TEST_F(Vdp2CompositorTest, UT001_TransFlagDecode) {
    // enable only -> no trans (neither 0x200 nor 0x100 condition fully met).
    EXPECT_FALSE(decode(vdp2cc::kCcEnNBG0).perLayer[vdp2cc::kNBG0].transFlag);
    // enable + CCRTMD(0x200) -> trans.
    EXPECT_TRUE(decode(vdp2cc::kCcEnNBG0 | vdp2cc::kCcrtmd)
                    .perLayer[vdp2cc::kNBG0].transFlag);
    // enable + CCMD(0x100) -> trans.
    EXPECT_TRUE(decode(vdp2cc::kCcEnNBG0 | vdp2cc::kCcmd)
                    .perLayer[vdp2cc::kNBG0].transFlag);
}

// UT-002: per-layer ccRatio expansion matches vdp2_color_oracle.h
// decodeLayerAlpha (the vidsoft baseline) for every CCCTL/ratio pattern that
// makes the layer enabled. Sweep all 32 ratio values in both fields.
TEST_F(Vdp2CompositorTest, UT002_RatioMatchesOracleNBG0NBG1) {
    for (int r = 0; r <= 0x1F; ++r) {
        // NBG0: CCRNA field 0; enable bit 0x01.
        uint16_t ccrna0 = static_cast<uint16_t>(r);
        uint16_t cctl = vdp2cc::kCcEnNBG0;
        State s = decode(cctl, ccrna0);
        uint8_t expect = decodeLayerAlpha(cctl, ccrna0, /*field*/ 0,
                                          /*enable*/ 0x01, /*ext*/ 0x200) & 0x3F;
        EXPECT_EQ(s.perLayer[vdp2cc::kNBG0].ratio, expect) << "NBG0 r=" << r;

        // NBG1: CCRNA field 1; enable bit 0x02. Put value in high field.
        uint16_t ccrna1 = static_cast<uint16_t>(r << 8);
        cctl = vdp2cc::kCcEnNBG1;
        s = decode(cctl, ccrna1);
        expect = decodeLayerAlpha(cctl, ccrna1, /*field*/ 1,
                                  /*enable*/ 0x02, /*ext*/ 0x200) & 0x3F;
        EXPECT_EQ(s.perLayer[vdp2cc::kNBG1].ratio, expect) << "NBG1 r=" << r;
    }
}

TEST_F(Vdp2CompositorTest, UT002_RatioMatchesOracleNBG2NBG3RBG0) {
    for (int r = 0; r <= 0x1F; ++r) {
        uint16_t low = static_cast<uint16_t>(r);
        uint16_t high = static_cast<uint16_t>(r << 8);

        State s = decode(vdp2cc::kCcEnNBG2, /*ccrna*/ 0, /*ccrnb*/ low);
        EXPECT_EQ(s.perLayer[vdp2cc::kNBG2].ratio,
                  decodeLayerAlpha(vdp2cc::kCcEnNBG2, low, 0, 0x04, 0x200) & 0x3F)
            << "NBG2 r=" << r;

        s = decode(vdp2cc::kCcEnNBG3, 0, high);
        EXPECT_EQ(s.perLayer[vdp2cc::kNBG3].ratio,
                  decodeLayerAlpha(vdp2cc::kCcEnNBG3, high, 1, 0x08, 0x200) & 0x3F)
            << "NBG3 r=" << r;

        s = decode(vdp2cc::kCcEnRBG0, 0, 0, low);
        EXPECT_EQ(s.perLayer[vdp2cc::kRBG0].ratio,
                  decodeLayerAlpha(vdp2cc::kCcEnRBG0, low, 0, 0x10, 0x200) & 0x3F)
            << "RBG0 r=" << r;
    }
}

// UT-002: a disabled layer reports ratio 0x3F (vidsoft "else info.alpha=0x3F").
TEST_F(Vdp2CompositorTest, UT002_DisabledLayerRatioIs3F) {
    State s = decode(0);  // nothing enabled
    EXPECT_EQ(s.perLayer[vdp2cc::kNBG0].ratio, 0x3F);
    EXPECT_EQ(s.perLayer[vdp2cc::kRBG0].ratio, 0x3F);
}

// UT-002: sprite ratio table (CCRS{A..D}) matches oracle
// decodeSpriteColorCalcTable for arbitrary register values.
TEST_F(Vdp2CompositorTest, UT002_SpriteRatioTableMatchesOracle) {
    std::array<uint16_t, 4> ccrs = {0x1234, 0x00FF, 0xABCD, 0x0507};
    State s = decode(vdp2cc::kCcEnSprite, 0, 0, 0, ccrs);
    auto expect = decodeSpriteColorCalcTable(ccrs);
    for (int i = 0; i < 8; ++i) {
        EXPECT_EQ(s.spriteRatioTable[i], expect[i]) << "sprite table idx " << i;
    }
}

// UT-002: line color ratio (CCRLB) matches oracle decodeLineColorAlpha (no +1).
TEST_F(Vdp2CompositorTest, UT002_LineColorRatioMatchesOracle) {
    for (int r = 0; r <= 0x1F; ++r) {
        uint16_t ccrlb = static_cast<uint16_t>(r);
        State s = decode(0, 0, 0, 0, {0, 0, 0, 0}, ccrlb);
        EXPECT_EQ(s.lineColorRatio, decodeLineColorAlpha(ccrlb))
            << "CCRLB r=" << r;
    }
}

// UT-009: CCRTMD selects the ratio SOURCE operand (top vs second). The per-layer
// ratio VALUE is identical regardless of CCRTMD (vidsoft stores the same
// info.alpha); only ratioSource changes. Verify both: ratioSource flips and the
// stored ratio value is unchanged by CCRTMD.
TEST_F(Vdp2CompositorTest, UT009_CcrtmdRatioSourceSwitch) {
    const uint16_t ratioReg = 0x0005;  // arbitrary CCRNA value for NBG0
    // CCRTMD = 0 -> ratio from top.
    State top = decode(vdp2cc::kCcEnNBG0, ratioReg);
    EXPECT_EQ(top.ratioSource, vdp2cc::RatioSource::Top);
    // CCRTMD = 1 -> ratio from second.
    State second = decode(vdp2cc::kCcEnNBG0 | vdp2cc::kCcrtmd, ratioReg);
    EXPECT_EQ(second.ratioSource, vdp2cc::RatioSource::Second);

    // The per-layer ratio value itself is CCRTMD-independent: same ((~reg&0x1F)
    // <<1)+1 in both. (Note: with CCRTMD set, ccEnable also becomes true for the
    // layer; the ratio value computed for an enabled layer is the same number.)
    uint8_t valTop = top.perLayer[vdp2cc::kNBG0].ratio;
    uint8_t valSecond = second.perLayer[vdp2cc::kNBG0].ratio;
    EXPECT_EQ(valTop, valSecond);
    EXPECT_EQ(valTop, static_cast<uint8_t>((((~ratioReg) & 0x1F) << 1) + 1));
}

// UT-009: the ratio source maps to the titan blend operand selection vidsoft
// uses (VIDSoftVdp2DrawStart): CCMD(add) wins first, else CCRTMD picks
// TOP(0) vs BOTTOM/second(1). Confirm mode precedence (add overrides source).
TEST_F(Vdp2CompositorTest, UT009_AddModeIndependentOfRatioSource) {
    // CCMD set + CCRTMD set: mode is Add, ratioSource still decodes to Second
    // but the compositor uses Add (ratio ignored), matching titan ADD first.
    State s = decode(vdp2cc::kCcEnNBG0 | vdp2cc::kCcmd | vdp2cc::kCcrtmd);
    EXPECT_EQ(s.mode, vdp2cc::CalcMode::Add);
    EXPECT_EQ(s.ratioSource, vdp2cc::RatioSource::Second);
}

// ---------------------------------------------------------------------------
// T-008 real suite: basic-pattern composite (UT-005, UT-B01, UT-B02, UT-E03,
// UT-E04). These pin vdp2oracle::compositeBasic() as the authoritative
// host-port reference that the GLSL compositor (vdp2_basic_compositor.frag,
// Vdp2Compositor) reproduces 1:1: pick the highest-priority opaque layer color,
// else the back screen. No color calc (stages 2/3 / phase2).
//
// Layer attrs are built with the same packAttr layout the G-buffer stores
// (T-004 packGBufferAttr / T-006 packAttr). An "undrawn" slice is left at the
// render-pass clear value 0, which unpacks to priority 0 (transparent) and is
// therefore excluded -- exactly what UT-E03/E04 assert.
// ---------------------------------------------------------------------------

namespace basicpat {
// Convenience: build a 6-entry color + attr layer set, all transparent
// (attr 0). Tests fill the slots they exercise. Fully vdp2oracle-qualified
// because this section is below `using namespace vdp2cc;`, which makes the bare
// layer-id names (kNBG0..kSprite) ambiguous with vdp2cc::LayerIndex.
struct BasicLayers {
    std::array<uint32_t, vdp2oracle::kLayerCount> color{};
    std::array<uint32_t, vdp2oracle::kLayerCount> attr{};  // 0 -> prio 0 -> excluded

    void set(int layer, uint32_t rgb, uint8_t priority,
             bool transparent = false, bool ccEnable = false, uint8_t ccRatio = 0) {
        vdp2oracle::Attr a;
        a.priority = priority;
        a.transparent = transparent;
        a.ccEnable = ccEnable;
        a.ccRatio = ccRatio;
        color[layer] = rgb & 0x00FFFFFFu;
        attr[layer] = vdp2oracle::packAttr(a);
    }
};
}  // namespace basicpat

// UT-005: the highest-priority opaque layer wins; its color passes through
// unchanged (no color calc). Lower-priority opaque layers are ignored.
TEST_F(Vdp2CompositorTest, UT005_BasicPatternHighestPriorityWins) {
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, 0x00112233u, /*priority*/ 3);
    l.set(vdp2oracle::kRBG0, 0x00AABBCCu, /*priority*/ 6);  // highest -> wins
    l.set(vdp2oracle::kNBG2, 0x00445566u, /*priority*/ 1);

    uint32_t back = 0x00000000u;
    uint32_t out = vdp2oracle::compositeBasic(l.color, l.attr, back);
    EXPECT_EQ(out, 0x00AABBCCu);
}

// UT-B01: all six layers transparent (priority 0) -> back screen color.
TEST_F(Vdp2CompositorTest, UTB01_AllTransparentFallsBackToBack) {
    basicpat::BasicLayers l;  // everything priority 0
    uint32_t back = 0x00123456u;
    EXPECT_EQ(vdp2oracle::compositeBasic(l.color, l.attr, back), 0x00123456u);

    // Also covers the explicit transparent bit: an opaque-priority layer with
    // the transparent flag set is still excluded.
    basicpat::BasicLayers l2;
    l2.set(vdp2oracle::kNBG1, 0x00FFFFFFu, /*priority*/ 5, /*transparent*/ true);
    EXPECT_EQ(vdp2oracle::compositeBasic(l2.color, l2.attr, back), 0x00123456u);
}

// UT-B02: a single opaque layer outputs its color verbatim (no blend even when
// it is the only candidate).
TEST_F(Vdp2CompositorTest, UTB02_SingleLayerPassthrough) {
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG3, 0x00DEAD37u, /*priority*/ 4);
    uint32_t back = 0x00000001u;
    EXPECT_EQ(vdp2oracle::compositeBasic(l.color, l.attr, back), 0x00DEAD37u);
}

// UT-E03: an undrawn sprite slice (attr clear value 0 -> priority 0) is treated
// as transparent and does not affect compositing -- the opaque NBG0 still wins.
TEST_F(Vdp2CompositorTest, UTE03_UndrawnSpriteIgnored) {
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, 0x00203040u, /*priority*/ 5);
    // kSprite left at attr 0 (undrawn / transparent).
    EXPECT_EQ(l.attr[vdp2oracle::kSprite], 0u);
    uint32_t out = vdp2oracle::compositeBasic(l.color, l.attr, 0x00000000u);
    EXPECT_EQ(out, 0x00203040u);
}

// UT-E04: an undrawn color slice (priority 0) is excluded from selection; the
// next opaque layer is chosen, never the undrawn slice's stale color.
TEST_F(Vdp2CompositorTest, UTE04_UndrawnLayerExcludedFromSelect) {
    basicpat::BasicLayers l;
    // RBG0 has a leftover color but priority 0 (undrawn) -> must be skipped.
    l.color[vdp2oracle::kRBG0] = 0x00FFFFFFu;  // stale color, attr stays 0
    l.set(vdp2oracle::kNBG2, 0x00334455u, /*priority*/ 2);
    uint32_t out = vdp2oracle::compositeBasic(l.color, l.attr, 0x00000000u);
    EXPECT_EQ(out, 0x00334455u);  // NBG2, not the stale RBG0 white
}

// UT-B03 (basic-pattern flavour): same-priority tiebreak in compositeBasic
// matches selectSortedLayers (higher LayerId wins), so sprite beats NBG0.
TEST_F(Vdp2CompositorTest, UT005_BasicPatternTiebreakByLayerId) {
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, 0x00111111u, /*priority*/ 4);
    l.set(vdp2oracle::kSprite, 0x00222222u, /*priority*/ 4);  // tie; high id wins
    uint32_t out = vdp2oracle::compositeBasic(l.color, l.attr, 0x00000000u);
    EXPECT_EQ(out, 0x00222222u);
}

// ---------------------------------------------------------------------------
// T-012 real suite: normal color calc (UT-006, UT-007, UT-B04, UT-B05).
//
// These pin vdp2oracle::compositeNormal() (the host-port reference the GLSL
// compositor reproduces 1:1) against the established titan blend primitives
// (applyBlend / blendTop / blendBottom / blendAdd, already pinned to vidsoft by
// T-001). The top<->second selection reuses the basicpat::BasicLayers builder.
//
// Field mapping recap: compositeNormal reads ccEnable (attr bit 5) and ccRatio
// (attr bits 6..11) per layer; it builds titan pixels (alpha = ccRatio, bit 31
// = ccEnable) and runs the same blend as composite(). So the expected values
// below are computed by building the equivalent titan pixels and calling
// applyBlend, guaranteeing the oracle stays internally consistent.
// ---------------------------------------------------------------------------

namespace normalcc {
// Build the titan pixel compositeNormal reconstructs from a layer's color+attr.
uint32_t titanPixel(uint32_t rgb, uint8_t ccRatio, bool ccEnable) {
    uint32_t p = vdp2oracle::createPixel(static_cast<uint8_t>(ccRatio & 0x3F),
                                         vdp2oracle::getRed(rgb),
                                         vdp2oracle::getGreen(rgb),
                                         vdp2oracle::getBlue(rgb));
    if (ccEnable) p |= 0x80000000u;
    return p;
}
}  // namespace normalcc

// UT-006: ratio mode, CCRTMD = Top. The top layer has color calc enabled with a
// mid ratio; compositeNormal blends top<->second using the TOP layer's ratio
// (titan TITAN_BLEND_TOP). Result matches applyBlend(BlendMode::Top, ...).
TEST_F(Vdp2CompositorTest, UT006_NormalRatioTopSource) {
    const uint32_t topRgb = 0x00C0A040u;
    const uint32_t secondRgb = 0x00204080u;
    const uint8_t topRatio = 0x10;  // mid ratio -> blend runs (ratio < 0x3F)

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kRBG0, topRgb, /*priority*/ 6,
          /*transparent*/ false, /*ccEnable*/ true, topRatio);
    l.set(vdp2oracle::kNBG0, secondRgb, /*priority*/ 3);  // second, no cc

    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, /*back*/ 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false);

    uint32_t expect = vdp2oracle::applyBlend(
        vdp2oracle::BlendMode::Top,
        normalcc::titanPixel(topRgb, topRatio, true),
        normalcc::titanPixel(secondRgb, 0x3F, false)) & 0x00FFFFFFu;
    EXPECT_EQ(out, expect);
}

// UT-006: ratio mode, CCRTMD = Second. The ratio comes from the SECOND layer
// (titan TITAN_BLEND_BOTTOM); the gate is the top's trans bit (ccEnable).
TEST_F(Vdp2CompositorTest, UT006_NormalRatioSecondSource) {
    const uint32_t topRgb = 0x00FFFFFFu;
    const uint32_t secondRgb = 0x00102030u;
    const uint8_t secondRatio = 0x08;

    basicpat::BasicLayers l;
    // CCRTMD set -> both layers enabled; top supplies the trans bit, second the
    // ratio.
    l.set(vdp2oracle::kSprite, topRgb, /*priority*/ 7,
          /*transparent*/ false, /*ccEnable*/ true, /*topRatio*/ 0x1A);
    l.set(vdp2oracle::kRBG0, secondRgb, /*priority*/ 5,
          /*transparent*/ false, /*ccEnable*/ true, secondRatio);

    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, /*back*/ 0x00000000u,
        vdp2oracle::BlendMode::Bottom, /*ratioFromSecond*/ true);

    uint32_t expect = vdp2oracle::applyBlend(
        vdp2oracle::BlendMode::Bottom,
        normalcc::titanPixel(topRgb, 0x1A, true),
        normalcc::titanPixel(secondRgb, secondRatio, true)) & 0x00FFFFFFu;
    EXPECT_EQ(out, expect);
}

// UT-006: when color calc does NOT apply on top (ratio mode + Top source, top
// ratio 0x3F), the top color passes through unchanged (basic pattern).
// the SW reference: a cc-enabled top still blends at ratio 0x3F (fore_ratio 31 -> 31:1),
// not a pure passthrough. The blend gate is the top's cc-enable bit, not the
// ratio value. (The old titan +3/255 path treated ratio 0x3F as identity.)
TEST_F(Vdp2CompositorTest, UT006_RatioMaxStillBlends31To1) {
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, 0x00ABCDEFu, /*priority*/ 6,
          /*transparent*/ false, /*ccEnable*/ true, /*ratio*/ 0x3F);
    l.set(vdp2oracle::kNBG2, 0x00111111u, /*priority*/ 2);

    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false);
    uint32_t expect = vdp2oracle::blendTopSecondStage(
        0x00ABCDEFu, 0x3F, true, 0x00111111u, 0x3F, false,
        vdp2oracle::BlendMode::Top);
    EXPECT_EQ(out, expect);
}

// UT-007: add mode (CCMD = 1). top + second saturating add. Gate is the top
// trans bit (ccEnable). Result matches applyBlend(BlendMode::Add, ...).
TEST_F(Vdp2CompositorTest, UT007_NormalAddMode) {
    const uint32_t topRgb = 0x00405060u;
    const uint32_t secondRgb = 0x00102030u;

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kRBG0, topRgb, /*priority*/ 6,
          /*transparent*/ false, /*ccEnable*/ true, /*ratio*/ 0x10);
    l.set(vdp2oracle::kNBG1, secondRgb, /*priority*/ 4);

    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Add, /*ratioFromSecond*/ false);

    uint32_t expect = vdp2oracle::applyBlend(
        vdp2oracle::BlendMode::Add,
        normalcc::titanPixel(topRgb, 0x10, true),
        normalcc::titanPixel(secondRgb, 0x3F, false)) & 0x00FFFFFFu;
    EXPECT_EQ(out, expect);
    // Sanity: each channel is the plain sum here (no saturation).
    EXPECT_EQ(vdp2oracle::getRed(out), 0x50);    // 0x40 + 0x10
    EXPECT_EQ(vdp2oracle::getGreen(out), 0x70);  // 0x50 + 0x20
    EXPECT_EQ(vdp2oracle::getBlue(out), 0x90);   // 0x60 + 0x30
}

// UT-007: add mode does not run when the top trans bit is clear (ccEnable
// false) -> top passes through unchanged.
TEST_F(Vdp2CompositorTest, UT007_AddNoBlendWhenTopCcDisabled) {
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, 0x00405060u, /*priority*/ 6);  // ccEnable false
    l.set(vdp2oracle::kNBG1, 0x00102030u, /*priority*/ 4);

    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Add, /*ratioFromSecond*/ false);
    EXPECT_EQ(out, 0x00405060u);
}

// UT-007: the second operand is the back screen when only one opaque layer
// exists. The back screen has no color calc; add saturates against it.
TEST_F(Vdp2CompositorTest, UT007_AddWithBackAsSecond) {
    const uint32_t topRgb = 0x00203040u;
    const uint32_t back = 0x00102030u;

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, topRgb, /*priority*/ 6,
          /*transparent*/ false, /*ccEnable*/ true, /*ratio*/ 0x05);
    // no second opaque layer -> back screen fills the slot.

    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, back,
        vdp2oracle::BlendMode::Add, /*ratioFromSecond*/ false);

    uint32_t expect = vdp2oracle::applyBlend(
        vdp2oracle::BlendMode::Add,
        normalcc::titanPixel(topRgb, 0x05, true),
        normalcc::titanPixel(back, 0x3F, false)) & 0x00FFFFFFu;
    EXPECT_EQ(out, expect);
    EXPECT_EQ(vdp2oracle::getRed(out), 0x30);    // 0x20 + 0x10
    EXPECT_EQ(vdp2oracle::getGreen(out), 0x50);  // 0x30 + 0x20
    EXPECT_EQ(vdp2oracle::getBlue(out), 0x70);   // 0x40 + 0x30
}

// ---------------------------------------------------------------------------
// T-015 normal-path LNCL line color screen (titan.c:324). When the top opaque
// layer has the line color inserted (LNCLEN selects it), the top is blended
// with the per-row line color using the global mode -- independent of EXCCEN.
// These pin the darkening case the user reported missing.
// ---------------------------------------------------------------------------

// Top mode (CCRTMD=Top): a cc-enabled top with a mid ratio blends toward the
// (dark) line color, darkening the pixel. Matches titan blendTop(top, line).
TEST_F(Vdp2CompositorTest, UT015_LineColorTopModeDarkens) {
    const uint32_t topRgb = 0x00C0C0C0u;   // light grey
    const uint32_t lineRgb = 0x00000000u;  // black line color
    const uint8_t topRatio = 0x10;         // cc on, ratio < 0x3F -> blend runs

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG1, topRgb, /*priority*/ 4,
          /*transparent*/ false, /*ccEnable*/ true, topRatio);

    const int mask = 1 << vdp2oracle::kNBG1;
    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, /*back*/ 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        mask, lineRgb, /*lineColorAlpha*/ 0);

    // titan blendTop(top, line) with the top's ratio.
    uint32_t expect = vdp2oracle::blendTop(
        normalcc::titanPixel(topRgb, topRatio, true),
        normalcc::titanPixel(lineRgb, 0x3F, false)) & 0x00FFFFFFu;
    EXPECT_EQ(out, expect);
    // Darkened vs the un-inserted top (which would pass through at 0xC0C0C0).
    EXPECT_LT(vdp2oracle::getRed(out), 0xC0);
}

// Top mode with the top NOT color-calc'd (ratio 0x3F): blendTop is identity, so
// the line color has no effect even though LNCLEN selects the layer.
TEST_F(Vdp2CompositorTest, UT015_LineColorTopModeIdentityWhenTopOpaque) {
    const uint32_t topRgb = 0x00C0C0C0u;
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG1, topRgb, /*priority*/ 4,
          /*transparent*/ false, /*ccEnable*/ false, /*ratio*/ 0x3F);

    const int mask = 1 << vdp2oracle::kNBG1;
    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        mask, /*lineColor*/ 0x00000000u, /*lineColorAlpha*/ 0);
    EXPECT_EQ(out, topRgb);  // unchanged
}

// Bottom mode (CCRTMD=Second): the blend ratio comes from the line color
// (CCRLB), gated by the top's cc-enable. A dark line color darkens a cc-enabled
// top regardless of the top's own ratio.
TEST_F(Vdp2CompositorTest, UT015_LineColorBottomModeUsesLineAlpha) {
    const uint32_t topRgb = 0x00C0C0C0u;
    const uint32_t lineRgb = 0x00000000u;
    const uint8_t lineAlpha = 0x08;  // (CCRLB & 0x1F) << 1

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG1, topRgb, /*priority*/ 4,
          /*transparent*/ false, /*ccEnable*/ true, /*topRatio*/ 0x3F);

    const int mask = 1 << vdp2oracle::kNBG1;
    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Bottom, /*ratioFromSecond*/ true,
        mask, lineRgb, lineAlpha);

    uint32_t expect = vdp2oracle::blendBottom(
        normalcc::titanPixel(topRgb, 0x3F, true),
        normalcc::titanPixel(lineRgb, lineAlpha, false)) & 0x00FFFFFFu;
    EXPECT_EQ(out, expect);
    EXPECT_LT(vdp2oracle::getRed(out), 0xC0);  // darkened
}

// Bottom mode but the top is NOT a cc layer (no trans bit): blendBottom short-
// circuits, so the line color is ignored.
TEST_F(Vdp2CompositorTest, UT015_LineColorBottomModeNoCcPassthrough) {
    const uint32_t topRgb = 0x00C0C0C0u;
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG1, topRgb, /*priority*/ 4);  // ccEnable false

    const int mask = 1 << vdp2oracle::kNBG1;
    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Bottom, /*ratioFromSecond*/ true,
        mask, /*lineColor*/ 0x00000000u, /*lineColorAlpha*/ 0x08);
    EXPECT_EQ(out, topRgb);  // unchanged
}

// The line color blend consumes the color-calc slot: the second opaque layer
// below the top must NOT also be blended in (titan makes the top opaque first).
TEST_F(Vdp2CompositorTest, UT015_LineColorSuppressesSecondBlend) {
    const uint32_t topRgb = 0x00C0C0C0u;
    const uint32_t secondRgb = 0x00FF0000u;  // would tint red if it leaked in
    const uint32_t lineRgb = 0x00000000u;

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG1, topRgb, /*priority*/ 4,
          /*transparent*/ false, /*ccEnable*/ true, /*topRatio*/ 0x10);
    l.set(vdp2oracle::kNBG3, secondRgb, /*priority*/ 2,
          /*transparent*/ false, /*ccEnable*/ true, /*ratio*/ 0x10);

    const int mask = 1 << vdp2oracle::kNBG1;
    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        mask, lineRgb, 0);

    // Only top<->line; second (red) excluded -> the result is a pure grey scale.
    uint32_t expect = vdp2oracle::blendTop(
        normalcc::titanPixel(topRgb, 0x10, true),
        normalcc::titanPixel(lineRgb, 0x3F, false)) & 0x00FFFFFFu;
    EXPECT_EQ(out, expect);
}

// ---------------------------------------------------------------------------
// UT-GLSL: host transcription of the Vdp2Compositor fragment shader's
// blendTopSecondStage()/blendRatio()/blendAdd() (Vdp2Compositor.cpp kFragSrc).
// The standalone gtest target cannot run real GLSL, so a host/GLSL divergence
// (especially in the ADD branch) would otherwise slip past every unit test that
// only exercises the oracle. This transcribes the GLSL line-for-line and fuzzes
// it against vdp2oracle::compositeNormal() across all CCMD/CCRTMD modes, ratios
// and ccEnable combinations. Keep this in lock-step with kFragSrc: if the GLSL
// blend stage changes, change the transcription here too.
// ---------------------------------------------------------------------------
namespace glslreplica {

// GLSL: ivec3 blendRatio(ivec3 top, ivec3 bottom, int ccRatio) -- the SW reference >>5.
static void blendRatio(const int top[3], const int bottom[3], int ccRatio, int out[3]) {
    int fr = ccRatio >> 1;       // the SW reference fore_ratio (0..31)
    int sr = 0x20 - fr;
    for (int i = 0; i < 3; ++i) {
        out[i] = (top[i] * fr + bottom[i] * sr) >> 5;
    }
}

// GLSL: ivec3 blendAdd(ivec3 top, ivec3 bottom) -> min(top + bottom, 255)
static void blendAdd(const int top[3], const int bottom[3], int out[3]) {
    for (int i = 0; i < 3; ++i) {
        int v = top[i] + bottom[i];
        out[i] = (v > 255) ? 255 : v;
    }
}

// GLSL: ivec3 blendTopSecondStage(...) with u_colorCalcMode / u_ratioFromSecond.
// Returns the packed 0x00RRGGBB result.
static uint32_t blendTopSecondStage(uint32_t topColor, int topRatio, bool topCcEnable,
                                    uint32_t secondColor, int secondRatio,
                                    int u_colorCalcMode, int u_ratioFromSecond) {
    int top[3] = {(int)vdp2oracle::getRed(topColor), (int)vdp2oracle::getGreen(topColor),
                  (int)vdp2oracle::getBlue(topColor)};
    int second[3] = {(int)vdp2oracle::getRed(secondColor), (int)vdp2oracle::getGreen(secondColor),
                     (int)vdp2oracle::getBlue(secondColor)};

    bool doBlend = topCcEnable;  // the SW reference: blend iff top CCE
    if (!doBlend) {
        return topColor & 0x00FFFFFFu;
    }
    int out[3];
    if (u_colorCalcMode == 1) {
        blendAdd(top, second, out);
    } else {
        int ratio = (u_ratioFromSecond != 0) ? secondRatio : topRatio;
        blendRatio(top, second, ratio, out);
    }
    return vdp2oracle::createPixel(0, (uint8_t)out[0], (uint8_t)out[1], (uint8_t)out[2])
           & 0x00FFFFFFu;
}

// Reproduce the GLSL main()'s top/second selection + blend for a 6-layer set
// (no extended cc). Mirrors compositeNormal()'s scope.
static uint32_t composite(const std::array<uint32_t, vdp2oracle::kLayerCount>& color,
                          const std::array<uint32_t, vdp2oracle::kLayerCount>& attr,
                          uint32_t backColor,
                          int u_colorCalcMode, int u_ratioFromSecond,
                          int u_lineScreenMask = 0, uint32_t lineColor = 0,
                          int u_lineColorAlpha = 0,
                          int u_hiresCram12 = 0, int u_paletteFormatMask = 0) {
    int topIdx = -1, secondIdx = -1;
    for (int priority = 7; priority > 0 && secondIdx < 0; --priority) {
        for (int which = 5; which >= 0 && secondIdx < 0; --which) {
            vdp2oracle::Attr a = vdp2oracle::unpackAttr(attr[which]);
            if (a.transparent) continue;
            if (vdp2oracle::clampPriority(a.priority) == 0) continue;
            if ((int)vdp2oracle::clampPriority(a.priority) != priority) continue;
            if (topIdx < 0) topIdx = which; else secondIdx = which;
        }
    }
    if (topIdx < 0) return backColor & 0x00FFFFFFu;

    vdp2oracle::Attr topAttr = vdp2oracle::unpackAttr(attr[topIdx]);
    uint32_t topColor = color[topIdx] & 0x00FFFFFFu;
    int topRatio = topAttr.ccRatio & 0x3F;
    bool topCcEnable = topAttr.ccEnable;

    uint32_t secondColor;
    int secondRatio;
    if (secondIdx >= 0) {
        vdp2oracle::Attr s = vdp2oracle::unpackAttr(attr[secondIdx]);
        secondColor = color[secondIdx] & 0x00FFFFFFu;
        secondRatio = s.ccRatio & 0x3F;
    } else {
        secondColor = backColor & 0x00FFFFFFu;
        secondRatio = 0x3F;
    }

    // Normal-path LNCL line color screen (mirror of the GLSL main() block).
    // the SW reference inserts the line color only inside the CCE block, so this is gated
    // on the top layer's cc-enable.
    if (topCcEnable && ((u_lineScreenMask >> topIdx) & 1)) {
        int top[3] = {(int)vdp2oracle::getRed(topColor), (int)vdp2oracle::getGreen(topColor),
                      (int)vdp2oracle::getBlue(topColor)};
        int lc[3] = {(int)vdp2oracle::getRed(lineColor), (int)vdp2oracle::getGreen(lineColor),
                     (int)vdp2oracle::getBlue(lineColor)};
        int out[3] = {top[0], top[1], top[2]};
        if (u_colorCalcMode == 1) {
            blendAdd(top, lc, out);
        } else if (u_ratioFromSecond != 0) {
            if (topCcEnable) blendRatio(top, lc, u_lineColorAlpha, out);
        } else {
            blendRatio(top, lc, topRatio, out);
        }
        topColor = vdp2oracle::createPixel(0, (uint8_t)out[0], (uint8_t)out[1], (uint8_t)out[2])
                   & 0x00FFFFFFu;
        topRatio = 0x3F;
        topCcEnable = false;
    }

    // Hi-res CRAM12 (MIXIT_SPECIAL_HIRES_CRAM12): palette-format second -> second = top.
    if (u_hiresCram12 != 0 && secondIdx >= 0 &&
        ((u_paletteFormatMask >> secondIdx) & 1)) {
        secondColor = topColor;
    }

    return blendTopSecondStage(topColor, topRatio, topCcEnable,
                               secondColor, secondRatio, u_colorCalcMode, u_ratioFromSecond);
}

}  // namespace glslreplica

// UT-GLSL01: the transcribed GLSL blend stage matches vdp2oracle::compositeNormal
// for every (CCMD, CCRTMD) mode over a sweep of top/second ratios, ccEnable bits
// and colors. This is the regression that pins the GLSL ADD branch to the oracle
// (the live shader cannot be unit tested directly).
TEST_F(Vdp2CompositorTest, UTGLSL01_FragmentBlendMatchesOracle) {
    const uint32_t topColors[]   = {0x00405060u, 0x00FFFFFFu, 0x00010203u, 0x00F0C080u};
    const uint32_t secondColors[] = {0x00102030u, 0x00000000u, 0x0040A0C0u, 0x00FFFFFFu};
    const uint8_t ratios[] = {0x00, 0x05, 0x10, 0x1F, 0x3E, 0x3F};

    struct Mode { vdp2oracle::BlendMode oracleMode; int ccmd; int ratioFromSecond; bool fromSecond; };
    const Mode modes[] = {
        {vdp2oracle::BlendMode::Top,    0, 0, false},  // ratio + top
        {vdp2oracle::BlendMode::Bottom, 0, 1, true},   // ratio + second
        {vdp2oracle::BlendMode::Add,    1, 0, false},  // additive
        {vdp2oracle::BlendMode::Add,    1, 1, false},  // additive, CCRTMD set (ignored)
    };

    for (const auto& m : modes) {
        for (uint32_t tc : topColors) {
            for (uint32_t sc : secondColors) {
                for (uint8_t tr : ratios) {
                    for (uint8_t srr : ratios) {
                        for (int topCc = 0; topCc < 2; ++topCc) {
                            basicpat::BasicLayers l;
                            l.set(vdp2oracle::kNBG0, tc, /*priority*/ 6,
                                  /*transparent*/ false, /*ccEnable*/ topCc != 0, /*ratio*/ tr);
                            l.set(vdp2oracle::kNBG2, sc, /*priority*/ 3,
                                  /*transparent*/ false, /*ccEnable*/ true, /*ratio*/ srr);

                            uint32_t oracleOut = vdp2oracle::compositeNormal(
                                l.color, l.attr, /*back*/ 0x00000000u,
                                m.oracleMode, m.fromSecond);
                            uint32_t glslOut = glslreplica::composite(
                                l.color, l.attr, /*back*/ 0x00000000u,
                                m.ccmd, m.ratioFromSecond);

                            ASSERT_EQ(glslOut, oracleOut)
                                << "ccmd=" << m.ccmd << " rfs=" << m.ratioFromSecond
                                << " tc=" << std::hex << tc << " sc=" << sc
                                << " tr=" << (int)tr << " srr=" << (int)srr
                                << " topCc=" << topCc;
                        }
                    }
                }
            }
        }
    }
}

// UT-GLSL01b: the GLSL normal-path line color screen block matches the oracle's
// compositeNormal line color path across every (CCMD, CCRTMD) mode, top ratio,
// ccEnable and a sweep of line colors / line alphas. Pins the GLSL block added
// for T-015 (normal-path LNCL) to the host reference.
TEST_F(Vdp2CompositorTest, UTGLSL01b_LineColorMatchesOracle) {
    const uint32_t topColors[]  = {0x00C0C0C0u, 0x00FFFFFFu, 0x00204080u};
    const uint32_t lineColors[] = {0x00000000u, 0x00202020u, 0x00400000u};
    const uint8_t ratios[] = {0x00, 0x08, 0x1F, 0x3F};
    const uint8_t lineAlphas[] = {0x00, 0x0A, 0x3E};

    struct Mode { vdp2oracle::BlendMode oracleMode; int ccmd; int ratioFromSecond; bool fromSecond; };
    const Mode modes[] = {
        {vdp2oracle::BlendMode::Top,    0, 0, false},
        {vdp2oracle::BlendMode::Bottom, 0, 1, true},
        {vdp2oracle::BlendMode::Add,    1, 0, false},
    };

    for (const auto& m : modes) {
        for (uint32_t tc : topColors) {
            for (uint32_t lc : lineColors) {
                for (uint8_t tr : ratios) {
                    for (uint8_t la : lineAlphas) {
                        for (int topCc = 0; topCc < 2; ++topCc) {
                            basicpat::BasicLayers l;
                            l.set(vdp2oracle::kNBG1, tc, /*priority*/ 4,
                                  /*transparent*/ false, /*ccEnable*/ topCc != 0, /*ratio*/ tr);
                            const int mask = 1 << vdp2oracle::kNBG1;

                            uint32_t oracleOut = vdp2oracle::compositeNormal(
                                l.color, l.attr, /*back*/ 0x00000000u,
                                m.oracleMode, m.fromSecond, mask, lc, la);
                            uint32_t glslOut = glslreplica::composite(
                                l.color, l.attr, /*back*/ 0x00000000u,
                                m.ccmd, m.ratioFromSecond, mask, lc, la);

                            ASSERT_EQ(glslOut, oracleOut)
                                << "ccmd=" << m.ccmd << " rfs=" << m.ratioFromSecond
                                << " tc=" << std::hex << tc << " lc=" << lc
                                << " tr=" << (int)tr << " la=" << (int)la
                                << " topCc=" << topCc;
                        }
                    }
                }
            }
        }
    }
}

// UT-GLSL02: additive specifically fires only when the top layer's ccEnable bit
// (attr bit 5) is set, and produces a visible saturating add. This pins the ADD
// gate that the reported bug ("add not working") would violate: if the gate were
// (wrongly) something the top layer never satisfies in additive scenes, the GLSL
// would pass the top through unchanged here.
TEST_F(Vdp2CompositorTest, UTGLSL02_AddGateFiresOnTopCcEnable) {
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, 0x00203040u, /*priority*/ 6,
          /*transparent*/ false, /*ccEnable*/ true, /*ratio*/ 0x00);
    l.set(vdp2oracle::kNBG2, 0x00102030u, /*priority*/ 3);

    uint32_t fired = glslreplica::composite(l.color, l.attr, 0x00000000u,
                                            /*ccmd*/ 1, /*ratioFromSecond*/ 0);
    EXPECT_EQ(fired, 0x00305070u);  // saturating add of the two colors

    // ccEnable on top cleared -> add gate false -> top passes through.
    basicpat::BasicLayers l2;
    l2.set(vdp2oracle::kNBG0, 0x00203040u, /*priority*/ 6,
           /*transparent*/ false, /*ccEnable*/ false, /*ratio*/ 0x00);
    l2.set(vdp2oracle::kNBG2, 0x00102030u, /*priority*/ 3);
    uint32_t passthrough = glslreplica::composite(l2.color, l2.attr, 0x00000000u,
                                                  /*ccmd*/ 1, /*ratioFromSecond*/ 0);
    EXPECT_EQ(passthrough, 0x00203040u);
}

// UT-GLSL03 (G6): hi-res CRAM mode 1/2 (MIXIT_SPECIAL_HIRES_CRAM12). When the
// second image is palette format the blend is suppressed (second = top), so a
// cc-enabled top passes through. An RGB-format second still blends normally.
TEST_F(Vdp2CompositorTest, UTGLSL03_HiresCram12SuppressesPaletteSecond) {
    const uint32_t topRgb = 0x00C04020u;
    const uint32_t secondRgb = 0x00204060u;

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x10);
    l.set(vdp2oracle::kNBG2, secondRgb, 3, false, /*cc*/ true, /*ratio*/ 0x10);
    const int secondPalette = 1 << vdp2oracle::kNBG2;

    // Hi-res + palette second -> second forced to top -> blend of top with top
    // yields top (any ratio): passthrough.
    uint32_t supp = glslreplica::composite(
        l.color, l.attr, 0x00000000u, /*ccmd*/ 0, /*ratioFromSecond*/ 0,
        /*lineScreenMask*/ 0, /*lineColor*/ 0u, /*lineColorAlpha*/ 0,
        /*hiresCram12*/ 1, /*paletteMask*/ secondPalette);
    EXPECT_EQ(supp, topRgb);

    // RGB-format second (mask 0): the fold is not suppressed -> real blend.
    uint32_t blended = glslreplica::composite(
        l.color, l.attr, 0x00000000u, /*ccmd*/ 0, /*ratioFromSecond*/ 0,
        /*lineScreenMask*/ 0, /*lineColor*/ 0u, /*lineColorAlpha*/ 0,
        /*hiresCram12*/ 1, /*paletteMask*/ 0);
    EXPECT_NE(blended, topRgb);
}

// UT-B04: ratio boundaries (the SW reference >>5). fore_ratio = ratio >> 1: ratio 0 ->
// fr 0 (all second), ratio 0x1F -> fr 15 (~half), ratio 0x3F -> fr 31 (31:1).
// Match applyBlend (the oracle) for each case.
TEST_F(Vdp2CompositorTest, UTB04_RatioBoundaryMinAndMid) {
    const uint32_t topRgb = 0x00FFFFFFu;
    const uint32_t secondRgb = 0x00000000u;

    // ratio = 0 -> fore_ratio 0 -> result is entirely the second color.
    {
        basicpat::BasicLayers l;
        l.set(vdp2oracle::kNBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x00);
        l.set(vdp2oracle::kNBG2, secondRgb, 3);
        uint32_t out = vdp2oracle::compositeNormal(
            l.color, l.attr, 0x00000000u,
            vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false);
        uint32_t expect = vdp2oracle::applyBlend(
            vdp2oracle::BlendMode::Top,
            normalcc::titanPixel(topRgb, 0x00, true),
            normalcc::titanPixel(secondRgb, 0x3F, false)) & 0x00FFFFFFu;
        EXPECT_EQ(out, expect);
        // fore_ratio 0 -> top contributes nothing -> second (black).
        EXPECT_EQ(vdp2oracle::getRed(out), 0x00);
    }
    // ratio = 0x1F -> fore_ratio 15 -> (0xFF*15)>>5 = 0x77.
    {
        basicpat::BasicLayers l;
        l.set(vdp2oracle::kNBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x1F);
        l.set(vdp2oracle::kNBG2, secondRgb, 3);
        uint32_t out = vdp2oracle::compositeNormal(
            l.color, l.attr, 0x00000000u,
            vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false);
        uint32_t expect = vdp2oracle::applyBlend(
            vdp2oracle::BlendMode::Top,
            normalcc::titanPixel(topRgb, 0x1F, true),
            normalcc::titanPixel(secondRgb, 0x3F, false)) & 0x00FFFFFFu;
        EXPECT_EQ(out, expect);
        EXPECT_EQ(vdp2oracle::getRed(out), 0x77);  // (0xFF*15)>>5
    }
}

// UT-B04: ratio = 0x3F (max stored) -> fore_ratio 31 -> 31:1 blend (the SW reference),
// NOT a passthrough. The blend runs in both ratio modes when the top is
// cc-enabled (gate = top cc-enable). Match applyBlend (the oracle).
TEST_F(Vdp2CompositorTest, UTB04_RatioBoundaryMax) {
    const uint32_t topRgb = 0x00112233u;
    const uint32_t secondRgb = 0x00AABBCCu;

    // Top source: ratio 0x3F -> 31:1 blend of top and second.
    {
        basicpat::BasicLayers l;
        l.set(vdp2oracle::kNBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x3F);
        l.set(vdp2oracle::kNBG2, secondRgb, 3);
        uint32_t out = vdp2oracle::compositeNormal(
            l.color, l.attr, 0x00000000u,
            vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false);
        uint32_t expect = vdp2oracle::blendTopSecondStage(
            topRgb, 0x3F, true, secondRgb, 0x3F, false, vdp2oracle::BlendMode::Top);
        EXPECT_EQ(out, expect);
        EXPECT_NE(out, topRgb);  // not a passthrough
    }
    // Second source: second ratio 0x3F -> fore_ratio 31 -> 31:1 (top dominant).
    {
        basicpat::BasicLayers l;
        l.set(vdp2oracle::kNBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x10);
        l.set(vdp2oracle::kNBG2, secondRgb, 3, false, /*cc*/ true, /*ratio*/ 0x3F);
        uint32_t out = vdp2oracle::compositeNormal(
            l.color, l.attr, 0x00000000u,
            vdp2oracle::BlendMode::Bottom, /*ratioFromSecond*/ true);
        uint32_t expect = vdp2oracle::applyBlend(
            vdp2oracle::BlendMode::Bottom,
            normalcc::titanPixel(topRgb, 0x10, true),
            normalcc::titanPixel(secondRgb, 0x3F, true)) & 0x00FFFFFFu;
        EXPECT_EQ(out, expect);
    }
}

// UT-B05: additive saturation. High-luminance top + second clamps each channel
// at 0xFF and never overflows.
TEST_F(Vdp2CompositorTest, UTB05_AddSaturation) {
    const uint32_t topRgb = 0x00F0C080u;
    const uint32_t secondRgb = 0x0040A0C0u;  // R 0xF0+0x40=0x130, G/B also > 0xFF

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x10);
    l.set(vdp2oracle::kNBG2, secondRgb, 3);

    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Add, /*ratioFromSecond*/ false);

    uint32_t expect = vdp2oracle::applyBlend(
        vdp2oracle::BlendMode::Add,
        normalcc::titanPixel(topRgb, 0x10, true),
        normalcc::titanPixel(secondRgb, 0x3F, false)) & 0x00FFFFFFu;
    EXPECT_EQ(out, expect);
    // Each channel saturates at 0xFF.
    EXPECT_EQ(vdp2oracle::getRed(out), 0xFF);    // 0xF0 + 0x40 -> clamp
    EXPECT_EQ(vdp2oracle::getGreen(out), 0xFF);  // 0xC0 + 0xA0 -> clamp
    EXPECT_EQ(vdp2oracle::getBlue(out), 0xFF);   // 0x80 + 0xC0 -> clamp
}

// UT-B05: full white + full white saturates to white, not wrap to black.
TEST_F(Vdp2CompositorTest, UTB05_AddWhitePlusWhite) {
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, 0x00FFFFFFu, 6, false, /*cc*/ true, /*ratio*/ 0x00);
    l.set(vdp2oracle::kNBG2, 0x00FFFFFFu, 3);
    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Add, /*ratioFromSecond*/ false);
    EXPECT_EQ(out, 0x00FFFFFFu);
}

// T-012: when the top layer is the only opaque layer and color calc is enabled
// in ratio mode, the second operand is the back screen (matches compositeBasic
// fallthrough but with a blend applied).
TEST_F(Vdp2CompositorTest, UT006_SingleLayerBlendsAgainstBack) {
    const uint32_t topRgb = 0x00FFFFFFu;
    const uint32_t back = 0x00000000u;
    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x00);

    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, back,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false);
    uint32_t expect = vdp2oracle::applyBlend(
        vdp2oracle::BlendMode::Top,
        normalcc::titanPixel(topRgb, 0x00, true),
        normalcc::titanPixel(back, 0x3F, false)) & 0x00FFFFFFu;
    EXPECT_EQ(out, expect);
}

// T-012: all-transparent still falls back to the back screen color (no blend).
TEST_F(Vdp2CompositorTest, UT006_AllTransparentFallsBackToBack) {
    basicpat::BasicLayers l;  // everything transparent
    uint32_t out = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00654321u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false);
    EXPECT_EQ(out, 0x00654321u);
}

// ---------------------------------------------------------------------------
// T-009 real suite: sprite decode (UT-010). Pins vdp2_sprite_decode.h
// decodeSprite() as the authoritative host-port reference the GLSL sprite pass
// (Vdp2SpriteDecoder fragment shader) reproduces 1:1. Inputs are VDP1
// framebuffer pixels in the renderer's alpha-byte encoding (encodeAlphaByte:
// bit7 show, bit6 palette, bits5:3 colorcalc code, bits2:0 priority slot) plus
// the SPCTL/CCCTL sprite registers. Outputs: priority (post PRISA table),
// ccEnable / ccRatio, color. Shadow is phase2; only the flag is decoded.
// ---------------------------------------------------------------------------

using vdp2sprite::FbPixel;
using vdp2sprite::SpriteRegs;
using vdp2sprite::Decoded;

namespace spritedec {
// Build a sprite-regs block: priority slot N -> priority value, ratio code N ->
// ratio value, identity by default; tests override the bits they exercise.
SpriteRegs makeRegs() {
    SpriteRegs r;
    for (int i = 0; i < 8; ++i) {
        r.priorityTable[i] = static_cast<uint8_t>(i);   // slot i -> priority i
        r.spriteRatioTable[i] = static_cast<uint8_t>(0x10 + i);
    }
    return r;
}
// Compose an alpha byte the way the rasterizer does.
uint8_t alphaByte(bool show, bool palette, uint8_t ccCode, uint8_t prioSlot) {
    return static_cast<uint8_t>((show ? 0x80 : 0)
                              | (palette ? 0x40 : 0)
                              | ((ccCode & 0x7) << 3)
                              | (prioSlot & 0x7));
}
}  // namespace spritedec

// UT-010: show bit clear -> the pixel is transparent (slice keeps clear value).
TEST_F(Vdp2CompositorTest, UT010_ShowBitClearIsTransparent) {
    SpriteRegs r = spritedec::makeRegs();
    FbPixel px;
    px.r = 0x12; px.g = 0x34; px.b = 0x56;
    px.a = spritedec::alphaByte(/*show*/ false, /*palette*/ false, 0, 4);
    Decoded d = vdp2sprite::decodeSprite(px, r, cram.data());
    EXPECT_TRUE(d.transparent);
}

// UT-010: a direct-color sprite passes its RGB through; priority comes from the
// PRISA table indexed by the framebuffer priority slot.
TEST_F(Vdp2CompositorTest, UT010_DirectColorPriorityFromTable) {
    SpriteRegs r = spritedec::makeRegs();
    r.priorityTable[5] = 6;  // slot 5 -> priority 6
    FbPixel px;
    px.r = 0xAA; px.g = 0xBB; px.b = 0xCC;
    px.a = spritedec::alphaByte(/*show*/ true, /*palette*/ false, /*cc*/ 0, /*slot*/ 5);
    Decoded d = vdp2sprite::decodeSprite(px, r, cram.data());
    EXPECT_FALSE(d.transparent);
    EXPECT_EQ(d.priority, 6);
    EXPECT_EQ(d.color, 0x00AABBCCu);
    EXPECT_FALSE(d.ccEnable);  // CCCTL cc-window bit (0x40) not set
}

// UT-010: priority slot resolving to 0 (titan skips priority 0) -> transparent.
TEST_F(Vdp2CompositorTest, UT010_PriorityZeroIsTransparent) {
    SpriteRegs r = spritedec::makeRegs();
    r.priorityTable[2] = 0;  // slot 2 -> priority 0
    FbPixel px;
    px.r = 0x10; px.g = 0x20; px.b = 0x30;
    px.a = spritedec::alphaByte(true, false, 0, /*slot*/ 2);
    Decoded d = vdp2sprite::decodeSprite(px, r, cram.data());
    EXPECT_TRUE(d.transparent);
}

// UT-010: an index-color sprite fetches its color from CRAM (same color the
// drawfb shader texelFetches), with the color RAM offset applied.
TEST_F(Vdp2CompositorTest, UT010_IndexColorFetchesFromCram) {
    SpriteRegs r = spritedec::makeRegs();
    r.priorityTable[3] = 5;
    r.colorRamOffset = 0;
    r.colorMode = 0;  // RGB555
    setCramWord(/*idx*/ 0x42, 0x7FFF);  // white
    FbPixel px;
    // palette index 0x42 -> low byte in R, high byte in G.
    px.r = 0x42; px.g = 0x00; px.b = 0x00;
    px.a = spritedec::alphaByte(true, /*palette*/ true, /*cc*/ 0, /*slot*/ 3);
    Decoded d = vdp2sprite::decodeSprite(px, r, cram.data());
    EXPECT_FALSE(d.transparent);
    EXPECT_EQ(d.priority, 5);
    EXPECT_EQ(vdp2oracle::getRed(d.color), 0xF8);
    EXPECT_EQ(vdp2oracle::getGreen(d.color), 0xF8);
    EXPECT_EQ(vdp2oracle::getBlue(d.color), 0xF8);
}

// UT-010: index 0 with the priority slot 0 -> transparent (drawfb 0-data rule).
TEST_F(Vdp2CompositorTest, UT010_IndexZeroSlotZeroTransparent) {
    SpriteRegs r = spritedec::makeRegs();  // slot 0 -> priority 0 anyway
    FbPixel px;
    px.r = 0x00; px.g = 0x00; px.b = 0x00;
    px.a = spritedec::alphaByte(true, /*palette*/ true, 0, /*slot*/ 0);
    Decoded d = vdp2sprite::decodeSprite(px, r, cram.data());
    EXPECT_TRUE(d.transparent);
}

// UT-010: color calc enabled. With CCCTL cc-window bit set and SPCCCS=2
// (priority >= SPCCN), a high-priority sprite gets ccEnable and the ratio from
// the sprite ratio table indexed by the color-calc code.
TEST_F(Vdp2CompositorTest, UT010_ColorCalcEnabledRatioFromTable) {
    SpriteRegs r = spritedec::makeRegs();
    r.priorityTable[6] = 5;          // slot 6 -> priority 5
    r.ccctl = 0x40;                  // sprite cc-window enable
    r.spctl = (2u << 12) | (3u << 8);  // SPCCCS=2 (>=), SPCCN=3
    r.spriteRatioTable[2] = 0x2A;    // color-calc code 2 -> ratio 0x2A
    FbPixel px;
    px.r = 0x11; px.g = 0x22; px.b = 0x33;
    px.a = spritedec::alphaByte(true, /*palette*/ false, /*cc code*/ 2, /*slot*/ 6);
    Decoded d = vdp2sprite::decodeSprite(px, r, cram.data());
    EXPECT_FALSE(d.transparent);
    EXPECT_EQ(d.priority, 5);
    EXPECT_TRUE(d.ccEnable);        // priority 5 >= SPCCN 3
    EXPECT_EQ(d.ccRatio, 0x2A);
}

// UT-010: SPCCCS=2 transparency test FAILS when priority < SPCCN -> cc disabled.
TEST_F(Vdp2CompositorTest, UT010_ColorCalcDisabledWhenTestFails) {
    SpriteRegs r = spritedec::makeRegs();
    r.priorityTable[1] = 2;           // priority 2
    r.ccctl = 0x40;
    r.spctl = (2u << 12) | (5u << 8); // SPCCCS=2 (>=), SPCCN=5
    FbPixel px;
    px.r = 0x10; px.g = 0x10; px.b = 0x10;
    px.a = spritedec::alphaByte(true, false, /*cc code*/ 1, /*slot*/ 1);
    Decoded d = vdp2sprite::decodeSprite(px, r, cram.data());
    EXPECT_FALSE(d.transparent);
    EXPECT_FALSE(d.ccEnable);  // 2 < 5 -> cc does not apply
    EXPECT_EQ(d.ccRatio, 0x3F);
}

// UT-010: the decoded sprite packs into the same attr layout the G-buffer stores
// and the compositor sorts it alongside background layers. An opaque sprite at
// priority 6 wins over an NBG0 at priority 3 via compositeBasic.
TEST_F(Vdp2CompositorTest, UT010_DecodedSpriteSortsWithLayers) {
    SpriteRegs r = spritedec::makeRegs();
    r.priorityTable[4] = 6;
    FbPixel px;
    px.r = 0x77; px.g = 0x88; px.b = 0x99;
    px.a = spritedec::alphaByte(true, false, 0, /*slot*/ 4);
    Decoded d = vdp2sprite::decodeSprite(px, r, cram.data());
    ASSERT_FALSE(d.transparent);

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kNBG0, 0x00112233u, /*priority*/ 3);
    l.color[vdp2oracle::kSprite] = d.color & 0x00FFFFFFu;
    l.attr[vdp2oracle::kSprite] = vdp2sprite::packSpriteAttr(d);

    uint32_t out = vdp2oracle::compositeBasic(l.color, l.attr, 0x00000000u);
    EXPECT_EQ(out, 0x00778899u);  // sprite (prio 6) beats NBG0 (prio 3)
}

// ---------------------------------------------------------------------------
// T-011 real suite: CRAM 3-mode color fetch (UT-012, UT-013, UT-014).
//
// FINDING (T-011): the new G-buffer path does NOT re-decode CRAM per mode in
// GLSL. CRAM is decoded ONCE on write into a fixed RGBA8 texture by
// VIDVulkan::onUpdateColorRamWord() (VIDVulkan.cpp:6733): modes 0/1 use
// SAT2YAB1 (RGB555 word), mode 2 uses SAT2YAB2 (RGB888 long). Both the existing
// layer/sprite GLSL shaders and the new compositor texelFetch this pre-decoded
// texture, so the GLSL side is CRAM-mode-INDEPENDENT. The mode resolution lives
// entirely in the CPU upload path, which is a direct counterpart of vidsoft.c
// Vdp2ColorRamGetColorSoft() -- the same logic this oracle's cramGetColor()
// ports. These UTs therefore pin cramGetColor() (the host-port reference for the
// upload-time decode) against the vidsoft baseline for all three modes, so a
// regression in either side is caught.
//
// Each case asserts the exact RGB the oracle returns for a known CRAM entry,
// using the documented RGB555 5->8 bit expansion (v<<3) for modes 0/1 and the
// verbatim RGB888 long for mode 2, and round-trips a value through the sprite
// decode path (the actual new-path consumer) to prove an index-color sprite
// pixel resolves to the same CRAM color in that mode.
// ---------------------------------------------------------------------------

namespace cram3 {
// RGB555 (5 bits/channel) -> the oracle's 0x00RRGGBB 8-bit value: each channel
// is expanded by <<3 (low 3 bits left zero), matching SAT2YAB1 and vidsoft.
uint32_t expandRgb555(uint8_t r5, uint8_t g5, uint8_t b5) {
    uint8_t r8 = static_cast<uint8_t>((r5 & 0x1F) << 3);
    uint8_t g8 = static_cast<uint8_t>((g5 & 0x1F) << 3);
    uint8_t b8 = static_cast<uint8_t>((b5 & 0x1F) << 3);
    return vdp2oracle::createPixel(0, r8, g8, b8);
}
// Compose an RGB555 word: bits 0-4 blue, 5-9 green, 10-14 red, bit 15 MSB.
uint16_t makeRgb555(uint8_t r5, uint8_t g5, uint8_t b5, bool msb = false) {
    return static_cast<uint16_t>((b5 & 0x1F)
                               | ((g5 & 0x1F) << 5)
                               | ((r5 & 0x1F) << 10)
                               | (msb ? 0x8000 : 0));
}
}  // namespace cram3

// UT-012: CRAM mode 0 (RGB555, 1024 colors). Word entries; cramGetColor expands
// each RGB555 channel by <<3 and matches vidsoft. The 1024-color space uses CRAM
// indices 0..0x3FF (byte addr 0..0x7FF), all inside the oracle's 0xFFF mask.
TEST_F(Vdp2CompositorTest, UT012_CramMode0Rgb555_1024) {
    struct Sample { uint8_t r5, g5, b5; };
    const Sample samples[] = {
        {0x1F, 0x1F, 0x1F},  // white
        {0x00, 0x00, 0x00},  // black
        {0x1F, 0x00, 0x00},  // red
        {0x00, 0x1F, 0x00},  // green
        {0x00, 0x00, 0x1F},  // blue
        {0x15, 0x0A, 0x1B},  // arbitrary
    };
    // Spread the samples across the 1024-color index range, including the top
    // index 0x3FF to exercise the addressing at the edge of the 1024 space.
    const uint32_t indices[] = {0u, 1u, 2u, 0x100u, 0x2ABu, 0x3FFu};

    for (int i = 0; i < 6; ++i) {
        const Sample& s = samples[i];
        uint32_t idx = indices[i];
        setCramWord(idx, cram3::makeRgb555(s.r5, s.g5, s.b5));
        uint32_t got = cramGetColor(/*colorMode*/ 0, idx, cram.data());
        uint32_t want = cram3::expandRgb555(s.r5, s.g5, s.b5);
        EXPECT_EQ(getRed(got), getRed(want))   << "mode0 idx=" << idx;
        EXPECT_EQ(getGreen(got), getGreen(want)) << "mode0 idx=" << idx;
        EXPECT_EQ(getBlue(got), getBlue(want))  << "mode0 idx=" << idx;
        // MSB clear for these samples -> bit 31 clear.
        EXPECT_EQ(got & 0x80000000u, 0u) << "mode0 idx=" << idx;
    }

    // MSB bit preserved at bit 31 (special color calc mode 3 marker), color 0.
    setCramWord(0x40, cram3::makeRgb555(0, 0, 0, /*msb*/ true));
    uint32_t msb = cramGetColor(/*colorMode*/ 0, 0x40, cram.data());
    EXPECT_EQ(msb & 0x00FFFFFFu, 0u);
    EXPECT_EQ(msb & 0x80000000u, 0x80000000u);
}

// UT-013: CRAM mode 1 (RGB555, 2048 colors). Same word decode as mode 0 (the
// only difference vs mode 0 is the available color count / palette addressing,
// not the per-entry decode -- vidsoft cases 0 and 1 are byte-identical). Verify
// indices across the full 2048-color range (0..0x7FF), which is the whole 4 KiB
// word space the oracle masks with 0xFFF.
TEST_F(Vdp2CompositorTest, UT013_CramMode1Rgb555_2048) {
    struct Sample { uint8_t r5, g5, b5; };
    const Sample samples[] = {
        {0x1F, 0x1F, 0x1F},
        {0x1F, 0x00, 0x00},
        {0x00, 0x1F, 0x00},
        {0x00, 0x00, 0x1F},
        {0x0C, 0x13, 0x07},
        {0x1E, 0x01, 0x10},
    };
    // Indices spanning the 2048-color range, including 0x400..0x7FF which are
    // ONLY reachable in mode 1 (mode 0 tops out at 0x3FF). This is what makes
    // the test mode-1-specific.
    const uint32_t indices[] = {0u, 0x3FFu, 0x400u, 0x555u, 0x6ABu, 0x7FFu};

    for (int i = 0; i < 6; ++i) {
        const Sample& s = samples[i];
        uint32_t idx = indices[i];
        setCramWord(idx, cram3::makeRgb555(s.r5, s.g5, s.b5));
        uint32_t got = cramGetColor(/*colorMode*/ 1, idx, cram.data());
        uint32_t want = cram3::expandRgb555(s.r5, s.g5, s.b5);
        EXPECT_EQ(getRed(got), getRed(want))   << "mode1 idx=" << idx;
        EXPECT_EQ(getGreen(got), getGreen(want)) << "mode1 idx=" << idx;
        EXPECT_EQ(getBlue(got), getBlue(want))  << "mode1 idx=" << idx;
    }

    // Mode 0 and mode 1 decode an identical word entry identically (the entry
    // decode does not depend on the mode; only the palette range differs).
    setCramWord(0x123, cram3::makeRgb555(0x11, 0x07, 0x1A));
    EXPECT_EQ(cramGetColor(0, 0x123, cram.data()),
              cramGetColor(1, 0x123, cram.data()));
}

// UT-014: CRAM mode 2 (RGB888, 1024 colors). Long (32-bit) entries returned
// verbatim by cramGetColor (vidsoft case 2 == T2ReadLong), so all 8 bits per
// channel survive with no expansion. 1024 long entries occupy byte addr
// 0..0xFFF, the full masked space.
TEST_F(Vdp2CompositorTest, UT014_CramMode2Rgb888_1024) {
    struct Sample { uint32_t rgb; };
    const Sample samples[] = {
        {0x00FFFFFFu},  // white, full 8-bit
        {0x00000000u},  // black
        {0x00FF0000u},  // red
        {0x0000FF00u},  // green
        {0x000000FFu},  // blue
        {0x00123456u},  // arbitrary (proves no RGB555 truncation)
    };
    // Long indices across the 1024-entry range, including 0x3FF (top).
    const uint32_t indices[] = {0u, 1u, 0x80u, 0x1FFu, 0x2A0u, 0x3FFu};

    for (int i = 0; i < 6; ++i) {
        uint32_t rgb = samples[i].rgb;
        uint32_t idx = indices[i];
        setCramLong(idx, rgb);
        uint32_t got = cramGetColor(/*colorMode*/ 2, idx, cram.data());
        EXPECT_EQ(getRed(got), (rgb >> 16) & 0xFF)  << "mode2 idx=" << idx;
        EXPECT_EQ(getGreen(got), (rgb >> 8) & 0xFF) << "mode2 idx=" << idx;
        EXPECT_EQ(getBlue(got), rgb & 0xFF)         << "mode2 idx=" << idx;
    }

    // Mode 2 keeps the low 3 bits that RGB555 (mode 0/1) would zero: a value
    // whose low bits are set must survive verbatim, proving mode 2 is NOT going
    // through the 5-bit path.
    setCramLong(0x10, 0x00070503u);  // low 3 bits set in every channel
    uint32_t fine = cramGetColor(2, 0x10, cram.data());
    EXPECT_EQ(getRed(fine), 0x07);
    EXPECT_EQ(getGreen(fine), 0x05);
    EXPECT_EQ(getBlue(fine), 0x03);
}

// UT-012/013/014 (consumer side): the actual new-path consumer of CRAM is the
// sprite decode (vdp2_sprite_decode.h decodeSprite, T-009) and the layer color
// fetch, both of which read the pre-decoded CRAM image via cramGetColor. Prove
// an index-color sprite pixel resolves to the correct CRAM color in each mode,
// so the 3-mode coverage reaches the path the compositor actually uses.
TEST_F(Vdp2CompositorTest, UT012_014_SpriteDecodeFetchesPerMode) {
    using vdp2sprite::FbPixel;
    using vdp2sprite::SpriteRegs;
    using vdp2sprite::Decoded;

    auto makeIndexPixel = [](uint32_t colindex) {
        FbPixel px;
        px.r = static_cast<uint8_t>(colindex & 0xFF);
        px.g = static_cast<uint8_t>((colindex >> 8) & 0xFF);
        px.b = 0;  // below the 0x73 shadow threshold
        px.a = spritedec::alphaByte(/*show*/ true, /*palette*/ true,
                                    /*cc*/ 0, /*slot*/ 4);
        return px;
    };

    // Mode 0 (RGB555).
    {
        SpriteRegs r = spritedec::makeRegs();
        r.priorityTable[4] = 5;
        r.colorRamOffset = 0;
        r.colorMode = 0;
        const uint32_t idx = 0x55;
        setCramWord(idx, cram3::makeRgb555(0x1F, 0x10, 0x08));
        Decoded d = vdp2sprite::decodeSprite(makeIndexPixel(idx), r, cram.data());
        ASSERT_FALSE(d.transparent);
        uint32_t want = cram3::expandRgb555(0x1F, 0x10, 0x08) & 0x00FFFFFFu;
        EXPECT_EQ(d.color, want);
    }
    // Mode 1 (RGB555, high palette index only reachable in mode 1).
    {
        SpriteRegs r = spritedec::makeRegs();
        r.priorityTable[4] = 5;
        r.colorRamOffset = 0;
        r.colorMode = 1;
        const uint32_t idx = 0x4AB;
        setCramWord(idx, cram3::makeRgb555(0x02, 0x1E, 0x11));
        Decoded d = vdp2sprite::decodeSprite(makeIndexPixel(idx), r, cram.data());
        ASSERT_FALSE(d.transparent);
        uint32_t want = cram3::expandRgb555(0x02, 0x1E, 0x11) & 0x00FFFFFFu;
        EXPECT_EQ(d.color, want);
    }
    // Mode 2 (RGB888): full 8-bit color survives the fetch.
    {
        SpriteRegs r = spritedec::makeRegs();
        r.priorityTable[4] = 5;
        r.colorRamOffset = 0;
        r.colorMode = 2;
        const uint32_t idx = 0x80;
        setCramLong(idx, 0x00123456u);
        Decoded d = vdp2sprite::decodeSprite(makeIndexPixel(idx), r, cram.data());
        ASSERT_FALSE(d.transparent);
        EXPECT_EQ(d.color, 0x00123456u);
    }
}

// ---------------------------------------------------------------------------
// T-014 real suite: extended color calc (UT-008, UT-B06).
//
// AUTHORITATIVE REFERENCE: the SW reference ss/vdp2_render.cpp T_MixIt(). vidsoft/titan
// do NOT implement this chain (2-layer model), so these pin
// vdp2oracle::compositeExtended() / buildExtendedSecond() (faithful the SW reference
// ports) as the host-port reference the GLSL compositor reproduces 1:1.
//
// Extended second (no line color inserted):
//   - secondCc && (CRAM0 || third ISRGB) -> ext second = (second + third) / 2
//   - otherwise                          -> ext second = second
// Then top <-> extended-second blend reuses the normal-cc stage (CCRNx / CCMD).
//
// The expected values are computed by composing the SAME extended-second math
// (buildExtendedSecond) + the SAME top<->second stage (blendTopSecondStage) the
// oracle exposes, keeping the oracle internally consistent and the GLSL pinned.
// ---------------------------------------------------------------------------

// UT-008: chain stops at second (second.ccEnable == 0). The extended second is
// just the second screen (ratio 4:0:0), so the result equals the normal-cc blend
// of top and second -- a third screen is present but NOT folded in.
TEST_F(Vdp2CompositorTest, UT008_ExtendedChainStopsAtSecond) {
    const uint32_t topRgb = 0x00C0A040u;
    const uint32_t secondRgb = 0x00204080u;
    const uint32_t thirdRgb = 0x00FF00FFu;  // present but must be ignored

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kRBG0, topRgb, /*priority*/ 6,
          /*transparent*/ false, /*ccEnable*/ true, /*ratio*/ 0x10);
    l.set(vdp2oracle::kNBG0, secondRgb, /*priority*/ 4,
          /*transparent*/ false, /*ccEnable*/ false, /*ratio*/ 0x3F);  // cc off
    l.set(vdp2oracle::kNBG1, thirdRgb, /*priority*/ 2);

    uint32_t out = vdp2oracle::compositeExtended(
        l.color, l.attr, /*back*/ 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false);

    // Extended second = second (4:0:0). Result == top<->second normal blend.
    uint32_t expect = vdp2oracle::blendTopSecondStage(
        topRgb, 0x10, true, secondRgb, 0x3F, false, vdp2oracle::BlendMode::Top);
    EXPECT_EQ(out, expect);
    // Also equals the plain normal-cc composite for this configuration.
    EXPECT_EQ(out, vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false));
}

// UT-008: chain reaches third (second.ccEnable == 1, no LNCL). The extended
// second is (second + third) / 2 (ratio 2:2:0); then top <-> extended-second.
TEST_F(Vdp2CompositorTest, UT008_ExtendedChainFoldsThird) {
    const uint32_t topRgb = 0x00804020u;
    const uint32_t secondRgb = 0x00406080u;
    const uint32_t thirdRgb = 0x0020A0E0u;

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kRBG0, topRgb, /*priority*/ 6,
          /*transparent*/ false, /*ccEnable*/ true, /*ratio*/ 0x08);
    l.set(vdp2oracle::kNBG0, secondRgb, /*priority*/ 4,
          /*transparent*/ false, /*ccEnable*/ true, /*ratio*/ 0x20);  // cc on
    l.set(vdp2oracle::kNBG1, thirdRgb, /*priority*/ 2,
          /*transparent*/ false, /*ccEnable*/ false, /*ratio*/ 0x3F);

    uint32_t out = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false);

    // No line, secondCc on, CRAM0 -> ext second = (second + third) / 2.
    uint32_t extSecond = vdp2oracle::buildExtendedSecond(
        secondRgb, thirdRgb, /*secondCc*/ true, /*secondRGB*/ true,
        /*thirdRGB*/ true, /*lineInsert*/ false, /*line*/ 0u, /*cram0*/ true);
    // Sanity: extended second is the per-channel average of second and third.
    EXPECT_EQ(vdp2oracle::getRed(extSecond),
              (vdp2oracle::getRed(secondRgb) + vdp2oracle::getRed(thirdRgb)) / 2);
    EXPECT_EQ(vdp2oracle::getBlue(extSecond),
              (vdp2oracle::getBlue(secondRgb) + vdp2oracle::getBlue(thirdRgb)) / 2);

    uint32_t expect = vdp2oracle::blendTopSecondStage(
        topRgb, 0x08, true, extSecond, 0x20, true, vdp2oracle::BlendMode::Top);
    EXPECT_EQ(out, expect);
    // The extended path differs from the plain normal-cc path (third folded in).
    EXPECT_NE(out, vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false));
}

// UT-008 (issue #22, Lunar regression): Table 12.2 extended-cc ratio for CRAM
// mode 1/2, no line inserted. The third image folds (2:2:0) ONLY when secondCc
// is set AND both the second and the third image are direct RGB. A palette
// second OR a palette third forces 4:0:0 (extended second = second). CRAM mode 0
// ignores the formats. This pins the fix for the previous code that folded on
// thirdRGB alone (and the even earlier code that folded on secondRGB alone),
// either of which broke the 4:0:0 cases the table requires.
TEST_F(Vdp2CompositorTest, UT008_ExtendedCram12FoldNeedsBothRgb) {
    const uint32_t second = 0x00406080u;
    const uint32_t third = 0x0020A0E0u;
    const uint32_t fold = vdp2oracle::halfSum(second, third);
    const uint32_t noFold = second & 0x00FFFFFFu;

    // CRAM mode 1/2 (cram0 == false): fold iff second AND third are RGB.
    EXPECT_EQ(vdp2oracle::buildExtendedSecond(
                  second, third, /*secondCc*/ true, /*secondRGB*/ true,
                  /*thirdRGB*/ true, /*lineInsert*/ false, 0u, /*cram0*/ false),
              fold);
    // Palette second -> 4:0:0 even with RGB third (the case the prior thirdRGB-
    // only code wrongly folded; the Lunar regression).
    EXPECT_EQ(vdp2oracle::buildExtendedSecond(
                  second, third, /*secondCc*/ true, /*secondRGB*/ false,
                  /*thirdRGB*/ true, /*lineInsert*/ false, 0u, /*cram0*/ false),
              noFold);
    // Palette third -> 4:0:0 even with RGB second.
    EXPECT_EQ(vdp2oracle::buildExtendedSecond(
                  second, third, /*secondCc*/ true, /*secondRGB*/ true,
                  /*thirdRGB*/ false, /*lineInsert*/ false, 0u, /*cram0*/ false),
              noFold);
    // secondCc off -> always 4:0:0.
    EXPECT_EQ(vdp2oracle::buildExtendedSecond(
                  second, third, /*secondCc*/ false, /*secondRGB*/ true,
                  /*thirdRGB*/ true, /*lineInsert*/ false, 0u, /*cram0*/ false),
              noFold);
    // CRAM mode 0 ignores formats: fold iff secondCc (palette operands fold too).
    EXPECT_EQ(vdp2oracle::buildExtendedSecond(
                  second, third, /*secondCc*/ true, /*secondRGB*/ false,
                  /*thirdRGB*/ false, /*lineInsert*/ false, 0u, /*cram0*/ true),
              fold);
}

// UT-008: line color inserted, CRAM mode 0 (the SW reference EXCC_LINE_CRAM0). The line
// color becomes the new second and the priority-second folds in at half
// luminance: ext2 = (lineColor + (secondCc ? second/2 : second)) / 2. The
// priority-third is NOT used in the CRAM0 line branch.
TEST_F(Vdp2CompositorTest, UT008_ExtendedLineColorCram0Fold) {
    const uint32_t secondRgb = 0x00808080u;
    const uint32_t thirdRgb = 0x00404040u;   // present but unused (CRAM0 line)
    const uint32_t lineRgb = 0x00C0C0C0u;

    // secondCc on: ext2 = (line + second/2)/2 = (0xC0 + 0x40)/2 = 0x80.
    uint32_t ext = vdp2oracle::buildExtendedSecond(
        secondRgb, thirdRgb, /*secondCc*/ true, /*secondRGB*/ true,
        /*thirdRGB*/ true, /*lineInsert*/ true, lineRgb, /*cram0*/ true);
    EXPECT_EQ(vdp2oracle::getRed(ext), 0x80);
    EXPECT_EQ(vdp2oracle::getGreen(ext), 0x80);
    EXPECT_EQ(vdp2oracle::getBlue(ext), 0x80);

    // secondCc off: second is folded full (no half-luminance) -> (0xC0+0x80)/2 = 0xA0.
    uint32_t extNoCc = vdp2oracle::buildExtendedSecond(
        secondRgb, thirdRgb, /*secondCc*/ false, /*secondRGB*/ true,
        /*thirdRGB*/ true, /*lineInsert*/ true, lineRgb, /*cram0*/ true);
    EXPECT_EQ(vdp2oracle::getRed(extNoCc), 0xA0);
}

// UT-008: extended cc with add mode (CCMD = 1). The extended second is built the
// same way; the top<->second stage saturating-adds.
TEST_F(Vdp2CompositorTest, UT008_ExtendedAddMode) {
    const uint32_t topRgb = 0x00203040u;
    const uint32_t secondRgb = 0x00102030u;
    const uint32_t thirdRgb = 0x00304050u;

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kRBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x10);
    l.set(vdp2oracle::kNBG0, secondRgb, 4, false, /*cc*/ true, /*ratio*/ 0x10);
    l.set(vdp2oracle::kNBG1, thirdRgb, 2);

    uint32_t out = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Add, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false);

    uint32_t extSecond = vdp2oracle::buildExtendedSecond(
        secondRgb, thirdRgb, /*secondCc*/ true, /*secondRGB*/ true,
        /*thirdRGB*/ true, /*lineInsert*/ false, /*line*/ 0u, /*cram0*/ true);
    uint32_t expect = vdp2oracle::blendTopSecondStage(
        topRgb, 0x10, true, extSecond, 0x10, true, vdp2oracle::BlendMode::Add);
    EXPECT_EQ(out, expect);
}

// UT-008: all transparent -> back screen (extended cc has nothing to compose).
TEST_F(Vdp2CompositorTest, UT008_ExtendedAllTransparentFallsBackToBack) {
    basicpat::BasicLayers l;
    uint32_t out = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00ABCDEFu,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false);
    EXPECT_EQ(out, 0x00ABCDEFu);
}

// UT-B06: extended chain boundary -- the chain length depends ONLY on the second
// (and, with LNCL, third) ccEnable bit, not on how many opaque layers exist.
// Same top/second/third colors, two cases: second.ccEnable off (chain stops,
// third ignored) vs on (third folded). The two outputs must differ exactly by
// the third fold.
TEST_F(Vdp2CompositorTest, UTB06_ChainLengthBoundaryBySecondCcEnable) {
    const uint32_t topRgb = 0x00FFFFFFu;
    const uint32_t secondRgb = 0x00200000u;
    const uint32_t thirdRgb = 0x00002000u;

    auto build = [&](bool secondCc) {
        basicpat::BasicLayers l;
        l.set(vdp2oracle::kRBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x00);
        l.set(vdp2oracle::kNBG0, secondRgb, 4, false, /*cc*/ secondCc, /*ratio*/ 0x3F);
        l.set(vdp2oracle::kNBG1, thirdRgb, 2);
        return vdp2oracle::compositeExtended(
            l.color, l.attr, 0x00000000u,
            vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
            /*lineColorInserted*/ false);
    };

    // second.ccEnable off: extended second = second -> blend top<->second.
    uint32_t outStop = build(false);
    uint32_t expectStop = vdp2oracle::blendTopSecondStage(
        topRgb, 0x00, true, secondRgb, 0x3F, false, vdp2oracle::BlendMode::Top);
    EXPECT_EQ(outStop, expectStop);

    // second.ccEnable on: extended second = (second+third)/2.
    uint32_t outFold = build(true);
    uint32_t extSecond = vdp2oracle::buildExtendedSecond(
        secondRgb, thirdRgb, /*secondCc*/ true, /*secondRGB*/ true,
        /*thirdRGB*/ true, /*lineInsert*/ false, /*line*/ 0u, /*cram0*/ true);
    uint32_t expectFold = vdp2oracle::blendTopSecondStage(
        topRgb, 0x00, true, extSecond, 0x3F, true, vdp2oracle::BlendMode::Top);
    EXPECT_EQ(outFold, expectFold);

    // The fold changes the second operand, so the composited results differ.
    EXPECT_NE(outStop, outFold);
}

// UT-B06: missing third operand. second.ccEnable on but only two opaque layers
// exist -> the third folds in as BLACK, halving the (single) second screen
// (ratio 2:2:0 with third = 0 -> (second + 0)/2). Verifies the boundary where
// the chain is requested but the extra screen is absent.
TEST_F(Vdp2CompositorTest, UTB06_MissingThirdFoldsBlack) {
    const uint32_t topRgb = 0x00000000u;       // black top so the blend is exact
    const uint32_t secondRgb = 0x00804020u;

    basicpat::BasicLayers l;
    // top with ratio 0 + cc on -> top<->extended blend runs; top black so the
    // result is purely the extended-second contribution scaled by the ratio.
    l.set(vdp2oracle::kRBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x3F);
    l.set(vdp2oracle::kNBG0, secondRgb, 4, false, /*cc*/ true, /*ratio*/ 0x3F);
    // no third opaque layer.

    uint32_t out = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false);

    // secondCc on, CRAM0, no third -> ext second = (second + black)/2 = second/2.
    uint32_t extSecond = vdp2oracle::buildExtendedSecond(
        secondRgb, 0u, /*secondCc*/ true, /*secondRGB*/ true,
        /*thirdRGB*/ false, /*lineInsert*/ false, /*line*/ 0u, /*cram0*/ true);
    // (second + black)/2 -> half the second screen.
    EXPECT_EQ(vdp2oracle::getRed(extSecond),   vdp2oracle::getRed(secondRgb) / 2);
    EXPECT_EQ(vdp2oracle::getGreen(extSecond), vdp2oracle::getGreen(secondRgb) / 2);
    EXPECT_EQ(vdp2oracle::getBlue(extSecond),  vdp2oracle::getBlue(secondRgb) / 2);

    uint32_t expect = vdp2oracle::blendTopSecondStage(
        topRgb, 0x3F, true, extSecond, 0x3F, true, vdp2oracle::BlendMode::Top);
    EXPECT_EQ(out, expect);
}

// UT-B06: EXCCEN disabled is the caller's responsibility -- when extended cc is
// off the compositor uses compositeNormal. Confirm the two oracle entry points
// agree for the chain-stops-at-second config (the only case where extended and
// normal must coincide: no third fold).
TEST_F(Vdp2CompositorTest, UTB06_ExtendedEqualsNormalWhenNoFold) {
    const uint32_t topRgb = 0x0011AA33u;
    const uint32_t secondRgb = 0x0044BB77u;

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kRBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x12);
    l.set(vdp2oracle::kNBG0, secondRgb, 4, false, /*cc*/ false, /*ratio*/ 0x3F);

    uint32_t ext = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false);
    uint32_t norm = vdp2oracle::compositeNormal(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false);
    EXPECT_EQ(ext, norm);
}

// ---------------------------------------------------------------------------
// T-015 real suite: LNCL line color screen insertion (UT-011).
//
// AUTHORITATIVE REFERENCE: the SW reference ss/vdp2_render.cpp T_MixIt() line-color
// insertion (EXCC_LINE_CRAM0 / EXCC_LINE_CRAM12). vidsoft/titan only model a
// 2-layer top+second compositor and do NOT implement this chain, so these pin
// vdp2oracle::compositeExtended()'s LNCL branch (a faithful the SW reference port the
// GLSL compositor reproduces 1:1).
//
// LNCL behaviour (the SW reference: the line color becomes the new second, the
// priority-second folds in -- it is KEPT, not excluded):
//   - line color ccEnable (LCCCEN) must be set for the line color to be inserted
//     (the port drives lineColorInserted from LCCCEN + LNCLEN + EXCCEN).
//   - CRAM mode 0:   ext second = (lineColor + (secondCc ? second/2 : second)) / 2
//   - CRAM mode 1/2: second RGB  -> (lineColor + ((secondCc && thirdRGB)
//                                    ? (second+third)/2 : second)) / 2
//                    second pal  -> lineColor   (priority-second excluded)
// When lineColorInserted is false the line color is ignored (T-014 behaviour).
// ---------------------------------------------------------------------------

// UT-011: LNCL inserted, CRAM mode 0 (the SW reference EXCC_LINE_CRAM0). The line color
// becomes the new second and the priority-second folds in at half luminance:
// ext2 = (line + second/2)/2. The priority-third / fourth screens are NOT used in
// the CRAM0 line branch.
TEST_F(Vdp2CompositorTest, UT011_LineColorInsertedFoldsSecond) {
    const uint32_t topRgb = 0x00FFFFFFu;
    const uint32_t secondRgb = 0x00800000u;
    const uint32_t thirdRgb = 0x00008000u;
    const uint32_t fourthRgb = 0x00000080u;  // present but unused (CRAM0 line)
    const uint32_t lineRgb = 0x00C0C0C0u;    // the inserted line color

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kRBG0, topRgb, 7, false, /*cc*/ true, /*ratio*/ 0x10);
    l.set(vdp2oracle::kNBG0, secondRgb, 6, false, /*cc*/ true, /*ratio*/ 0x20);
    l.set(vdp2oracle::kNBG1, thirdRgb, 5, false, /*cc*/ true, /*ratio*/ 0x3F);
    l.set(vdp2oracle::kNBG2, fourthRgb, 4);

    uint32_t out = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ true, /*lineColor*/ lineRgb);

    // ext2 = (line + second/2)/2 (CRAM0 line, secondCc on).
    uint32_t extSecond = vdp2oracle::buildExtendedSecond(
        secondRgb, thirdRgb, /*secondCc*/ true, /*secondRGB*/ true,
        /*thirdRGB*/ true, /*lineInsert*/ true, lineRgb, /*cram0*/ true);
    uint32_t expect = vdp2oracle::blendTopSecondStage(
        topRgb, 0x10, true, extSecond, 0x3F, true, vdp2oracle::BlendMode::Top);
    EXPECT_EQ(out, expect);

    // Inserting the line color changes the output vs the no-line chain
    // (no-line ext2 = (second + third)/2).
    uint32_t outNoLine = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false);
    EXPECT_NE(out, outNoLine);
}

// UT-011: LNCL inserted, CRAM mode 0 -> the priority-second image is KEPT (folded
// at half luminance with the line color), NOT excluded. the SW reference EXCC_LINE_CRAM0:
// ext2 = (line + second/2)/2. (This corrects the earlier manual-paraphrase model
// that wrongly excluded the priority-second.)
TEST_F(Vdp2CompositorTest, UT011_LineColorInsertedKeepsSecond) {
    const uint32_t topRgb = 0x00000000u;     // black top -> blend is exact
    const uint32_t secondRgb = 0x00804020u;
    const uint32_t lineRgb = 0x00204060u;

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kRBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x3F);
    l.set(vdp2oracle::kNBG0, secondRgb, 4, false, /*cc*/ true, /*ratio*/ 0x3F);
    // no opaque third screen -> third = black (unused in the CRAM0 line branch).

    uint32_t out = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ true, /*lineColor*/ lineRgb);

    // ext second = (line + second/2)/2 (CRAM0 line, secondCc on).
    uint32_t extSecond = vdp2oracle::buildExtendedSecond(
        secondRgb, 0u, /*secondCc*/ true, /*secondRGB*/ true,
        /*thirdRGB*/ false, /*lineInsert*/ true, lineRgb, /*cram0*/ true);
    uint32_t expect = vdp2oracle::blendTopSecondStage(
        topRgb, 0x3F, true, extSecond, 0x3F, true, vdp2oracle::BlendMode::Top);
    EXPECT_EQ(out, expect);

    // The priority-second screen DOES affect the result now: changing its color
    // changes the output (it is folded into the extended second, not excluded).
    basicpat::BasicLayers l2;
    l2.set(vdp2oracle::kRBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x3F);
    l2.set(vdp2oracle::kNBG0, 0x0000FF00u, 4, false, /*cc*/ true, /*ratio*/ 0x3F);
    uint32_t out2 = vdp2oracle::compositeExtended(
        l2.color, l2.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ true, /*lineColor*/ lineRgb);
    EXPECT_NE(out, out2);
}

// UT-011: CRAM mode 1/2 no-line fold is gated by the THIRD screen's format
// (the SW reference: secondCc && (CRAM0 || third ISRGB)). A palette-format third screen
// suppresses the fold (ext second = second), so the result equals the plain
// top<->second blend. In CRAM mode 0 the same config folds the third screen.
TEST_F(Vdp2CompositorTest, UT011_CramMode12PaletteThirdSuppressesFold) {
    const uint32_t topRgb = 0x00804020u;
    const uint32_t secondRgb = 0x00406080u;
    const uint32_t thirdRgb = 0x0020A0E0u;

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kRBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x08);
    l.set(vdp2oracle::kNBG0, secondRgb, 4, false, /*cc*/ true, /*ratio*/ 0x20);
    l.set(vdp2oracle::kNBG1, thirdRgb, 2, false, /*cc*/ false, /*ratio*/ 0x3F);

    // CRAM mode 1 + palette-format third -> fold suppressed (ext second = second).
    const uint32_t thirdPalette = 1u << vdp2oracle::kNBG1;
    uint32_t outSup = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false, /*lineColor*/ 0u,
        /*cramMode*/ 1, /*paletteMask*/ thirdPalette);
    uint32_t expectSup = vdp2oracle::blendTopSecondStage(
        topRgb, 0x08, true, secondRgb, 0x20, true, vdp2oracle::BlendMode::Top);
    EXPECT_EQ(outSup, expectSup);

    // CRAM mode 0: the fold runs regardless of format -> differs from suppressed.
    uint32_t outFold = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false, /*lineColor*/ 0u,
        /*cramMode*/ 0, /*paletteMask*/ thirdPalette);
    EXPECT_NE(outSup, outFold);
}

// UT-011: LNCL not inserted -> the line color argument is ignored entirely
// (the 6-arg and 7-arg overloads agree when lineColorInserted is false).
TEST_F(Vdp2CompositorTest, UT011_LineColorIgnoredWhenNotInserted) {
    const uint32_t topRgb = 0x00112233u;
    const uint32_t secondRgb = 0x00445566u;
    const uint32_t thirdRgb = 0x00778899u;

    basicpat::BasicLayers l;
    l.set(vdp2oracle::kRBG0, topRgb, 6, false, /*cc*/ true, /*ratio*/ 0x08);
    l.set(vdp2oracle::kNBG0, secondRgb, 4, false, /*cc*/ true, /*ratio*/ 0x20);
    l.set(vdp2oracle::kNBG1, thirdRgb, 2, false, /*cc*/ true, /*ratio*/ 0x3F);

    uint32_t withLine = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false, /*lineColor*/ 0x00FFFFFFu);
    uint32_t defaultLine = vdp2oracle::compositeExtended(
        l.color, l.attr, 0x00000000u,
        vdp2oracle::BlendMode::Top, /*ratioFromSecond*/ false,
        /*lineColorInserted*/ false);
    EXPECT_EQ(withLine, defaultLine);
}

// UT-011: line color ratio decode (CCRLB low 5 bits, no +1) is the per-line
// ratio source for LNCL. Pin decodeLineColorRatio (vdp2cc) against the oracle's
// decodeLineColorAlpha so the port and the host stay aligned for T-015.
TEST_F(Vdp2CompositorTest, UT011_LineColorRatioDecodeConsistent) {
    for (int r = 0; r <= 0x1F; ++r) {
        uint16_t ccrlb = static_cast<uint16_t>(r);
        EXPECT_EQ(vdp2cc::decodeLineColorRatio(ccrlb),
                  vdp2oracle::decodeLineColorAlpha(ccrlb))
            << "CCRLB=" << r;
    }
}

// ---------------------------------------------------------------------------
// G4: shadow + half-luminance. Pins vdp2oracle::selectLayersShadow() /
// halfLuminance() -- the reference renderer's PIX_DOSHAD "draw the layer below,
// darken it if it has SHADEN" behaviour the GLSL compositor reproduces.
// ---------------------------------------------------------------------------

// Helper: build a slice attr array, set one slice as a shadow caster, one as a
// shadow-accepting (SDCTL) background.
namespace {
inline uint32_t attrWord(int priority, bool doShadow, bool shadowEn) {
    vdp2oracle::Attr a;
    a.priority = static_cast<uint8_t>(priority);
    a.doShadow = doShadow;
    a.shadowEn = shadowEn;
    return vdp2oracle::packAttr(a);
}
}  // namespace

// A topmost shadow sprite over a SHADEN background: the background is displayed
// and darkened (halfLum), the shadow caster itself is dropped.
TEST_F(Vdp2CompositorTest, UTG4_ShadowOverShadenLayerHalves) {
    std::array<uint32_t, vdp2oracle::kLayerCount> attr;
    attr.fill(vdp2oracle::packAttr([] { vdp2oracle::Attr t; t.transparent = true; return t; }()));
    // Sprite (highest within priority) is the shadow caster at priority 6.
    attr[vdp2oracle::kSprite] = attrWord(/*prio*/ 6, /*doShadow*/ true, /*shadowEn*/ false);
    // NBG0 below it accepts shadows (SDCTL).
    attr[vdp2oracle::kNBG0] = attrWord(/*prio*/ 4, /*doShadow*/ false, /*shadowEn*/ true);

    int idx[4];
    bool halfLum = false;
    vdp2oracle::selectLayersShadow(attr, idx, halfLum);
    EXPECT_EQ(idx[0], vdp2oracle::kNBG0);  // shadow caster dropped -> NBG0 shown
    EXPECT_TRUE(halfLum);                  // NBG0 has SHADEN -> darkened
}

// Same, but the background does NOT accept shadows: it is displayed at full
// luminance (the shadow has no visible effect).
TEST_F(Vdp2CompositorTest, UTG4_ShadowOverNonShadenLayerNoHalve) {
    std::array<uint32_t, vdp2oracle::kLayerCount> attr;
    attr.fill(vdp2oracle::packAttr([] { vdp2oracle::Attr t; t.transparent = true; return t; }()));
    attr[vdp2oracle::kSprite] = attrWord(6, /*doShadow*/ true, /*shadowEn*/ false);
    attr[vdp2oracle::kNBG0] = attrWord(4, /*doShadow*/ false, /*shadowEn*/ false);

    int idx[4];
    bool halfLum = true;
    vdp2oracle::selectLayersShadow(attr, idx, halfLum);
    EXPECT_EQ(idx[0], vdp2oracle::kNBG0);
    EXPECT_FALSE(halfLum);
}

// A shadow caster that is NOT topmost (a higher-priority opaque layer is above
// it) casts no shadow: the topmost is displayed normally.
TEST_F(Vdp2CompositorTest, UTG4_ShadowHiddenWhenNotTopmost) {
    std::array<uint32_t, vdp2oracle::kLayerCount> attr;
    attr.fill(vdp2oracle::packAttr([] { vdp2oracle::Attr t; t.transparent = true; return t; }()));
    attr[vdp2oracle::kSprite] = attrWord(4, /*doShadow*/ true, /*shadowEn*/ false);
    attr[vdp2oracle::kNBG0] = attrWord(6, /*doShadow*/ false, /*shadowEn*/ true);  // above the shadow

    int idx[4];
    bool halfLum = true;
    vdp2oracle::selectLayersShadow(attr, idx, halfLum);
    EXPECT_EQ(idx[0], vdp2oracle::kNBG0);  // topmost (non-shadow) shown as-is
    EXPECT_FALSE(halfLum);
}

// Half-luminance is a per-channel right shift.
TEST_F(Vdp2CompositorTest, UTG4_HalfLuminanceMath) {
    uint32_t h = vdp2oracle::halfLuminance(0x00804020u);
    EXPECT_EQ(vdp2oracle::getRed(h), 0x40);
    EXPECT_EQ(vdp2oracle::getGreen(h), 0x20);
    EXPECT_EQ(vdp2oracle::getBlue(h), 0x10);
}

// G5: gradation blur = avg(avg(x-2, x-1), x), the causal 3-tap running blur the
// GLSL compositor computes (blurcake = (((g2+g1)>>1)+g0)>>1). Pins the per-channel
// integer math.
TEST_F(Vdp2CompositorTest, UTG5_GradationBlurMath) {
    auto blur = [](int g2, int g1, int g0) { return (((g2 + g1) >> 1) + g0) >> 1; };
    // Uniform input -> output equals the input (no change on a flat region).
    EXPECT_EQ(blur(0x40, 0x40, 0x40), 0x40);
    // x-2=0x00, x-1=0x40, x=0x80 -> avg(0x20, 0x80) = 0x50.
    EXPECT_EQ(blur(0x00, 0x40, 0x80), 0x50);
    // Weight check: the current sample dominates (1/2), the two prior share 1/4.
    EXPECT_EQ(blur(0x00, 0x00, 0xFF), (((0x00 + 0x00) >> 1) + 0xFF) >> 1);  // 0x7F
}

}  // namespace
