// Copyright 2026 devMiyax
// TDD test for vdp1c::collectDispatchIndices.
#include "gtest/gtest.h"
#include "../Vdp1ComputeCommands.h"
#include "../Vdp1ComputeMath.h"

#include <cstdint>
#include <vector>

TEST(DispatchUpTo, EmptyCmdsReturnsEmpty) {
    std::vector<Vdp1Cmd> cmds;
    auto out = vdp1c::collectDispatchIndices(cmds, 100u);
    EXPECT_TRUE(out.empty());
}

TEST(DispatchUpTo, AllPolygonAndDistortedAreDispatched) {
    std::vector<Vdp1Cmd> cmds(3);
    cmds[0].cmdType = VDP1C_TYPE_POLYGON;
    cmds[1].cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    cmds[2].cmdType = VDP1C_TYPE_POLYGON;

    auto out = vdp1c::collectDispatchIndices(cmds, UINT32_MAX);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0], 0u);
    EXPECT_EQ(out[1], 1u);
    EXPECT_EQ(out[2], 2u);
}

TEST(DispatchUpTo, OtherTypesAreSkipped) {
    std::vector<Vdp1Cmd> cmds(4);
    cmds[0].cmdType = VDP1C_TYPE_POLYGON;
    cmds[1].cmdType = VDP1C_TYPE_NOOP;
    cmds[2].cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    cmds[3].cmdType = VDP1C_TYPE_NOOP;

    auto out = vdp1c::collectDispatchIndices(cmds, UINT32_MAX);
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0], 0u);
    EXPECT_EQ(out[1], 2u);
}

TEST(DispatchUpTo, LastCmdIndexCutsOff) {
    std::vector<Vdp1Cmd> cmds(5);
    for (auto& c : cmds) c.cmdType = VDP1C_TYPE_POLYGON;

    auto out = vdp1c::collectDispatchIndices(cmds, 2u);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_EQ(out[0], 0u);
    EXPECT_EQ(out[2], 2u);
}

TEST(DispatchUpTo, LastCmdIndexZeroDispatchesOnlyFirst) {
    std::vector<Vdp1Cmd> cmds(3);
    for (auto& c : cmds) c.cmdType = VDP1C_TYPE_POLYGON;

    auto out = vdp1c::collectDispatchIndices(cmds, 0u);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], 0u);
}

TEST(DispatchUpTo, LastCmdIndexBeyondSizeIsClamped) {
    std::vector<Vdp1Cmd> cmds(2);
    for (auto& c : cmds) c.cmdType = VDP1C_TYPE_POLYGON;

    auto out = vdp1c::collectDispatchIndices(cmds, 999u);
    EXPECT_EQ(out.size(), 2u);
}

// =============================================================================
// computeBarrierMask (Phase A2: bbox-independent barrier batching)
// =============================================================================

namespace {
// Helper: build a polygon-typed Vdp1Cmd with a specific bbox.
Vdp1Cmd polyWithBbox(int x0, int y0, int x1, int y1) {
    Vdp1Cmd c{};
    c.cmdType = VDP1C_TYPE_POLYGON;
    c.bbox = glm::ivec4(x0, y0, x1, y1);
    return c;
}
int countBarriers(const std::vector<uint8_t>& mask) {
    int n = 0;
    for (auto b : mask) if (b) ++n;
    return n;
}
}  // namespace

TEST(BarrierMask, EmptyIndicesGivesEmptyMask) {
    std::vector<Vdp1Cmd> cmds;
    std::vector<uint32_t> indices;
    auto mask = vdp1c::computeBarrierMask(cmds, indices);
    EXPECT_TRUE(mask.empty());
}

TEST(BarrierMask, FirstDispatchAlwaysHasNoBarrier) {
    std::vector<Vdp1Cmd> cmds = {polyWithBbox(0, 0, 10, 10)};
    std::vector<uint32_t> indices = {0};
    auto mask = vdp1c::computeBarrierMask(cmds, indices);
    ASSERT_EQ(mask.size(), 1u);
    EXPECT_EQ(mask[0], 0u);  // first dispatch never gets a barrier
}

TEST(BarrierMask, NonOverlappingCommandsRunInOneBatch) {
    // 3 disjoint bboxes → 0 barriers (single batch, parallel dispatch)
    std::vector<Vdp1Cmd> cmds = {
        polyWithBbox( 0, 0,  10, 10),
        polyWithBbox(20, 0,  30, 10),
        polyWithBbox(40, 0,  50, 10),
    };
    std::vector<uint32_t> indices = {0, 1, 2};
    auto mask = vdp1c::computeBarrierMask(cmds, indices);
    EXPECT_EQ(countBarriers(mask), 0);
}

TEST(BarrierMask, OverlapForcesBarrier) {
    // [0] and [1] overlap → barrier before [1]. [2] disjoint from [1] → no
    // barrier (in-flight set was reset to {[1]} after barrier).
    std::vector<Vdp1Cmd> cmds = {
        polyWithBbox( 0,  0,  10, 10),
        polyWithBbox( 5,  5,  15, 15),  // overlaps [0]
        polyWithBbox(20, 20,  30, 30),  // disjoint from [1]
    };
    std::vector<uint32_t> indices = {0, 1, 2};
    auto mask = vdp1c::computeBarrierMask(cmds, indices);
    EXPECT_EQ(mask[0], 0u);
    EXPECT_EQ(mask[1], 1u);
    EXPECT_EQ(mask[2], 0u);
}

TEST(BarrierMask, BboxBoundaryTouchCountsAsOverlap) {
    // bbox a.maxX == b.minX → both rasterize the shared column → must be
    // serialized.
    std::vector<Vdp1Cmd> cmds = {
        polyWithBbox(0, 0, 10, 10),
        polyWithBbox(10, 0, 20, 10),  // touches at x=10
    };
    std::vector<uint32_t> indices = {0, 1};
    auto mask = vdp1c::computeBarrierMask(cmds, indices);
    EXPECT_EQ(mask[1], 1u);
}

TEST(BarrierMask, AllOverlappingChainGivesNMinus1Barriers) {
    // 5 cmds at the same bbox → every dispatch but the first needs barrier.
    std::vector<Vdp1Cmd> cmds(5, polyWithBbox(0, 0, 100, 100));
    std::vector<uint32_t> indices = {0, 1, 2, 3, 4};
    auto mask = vdp1c::computeBarrierMask(cmds, indices);
    EXPECT_EQ(countBarriers(mask), 4);  // = 5 - 1
}

TEST(BarrierMask, AfterBarrierOnlyNewBatchIsInFlight) {
    // [0] (0..10) and [1] (5..15) overlap → barrier before [1].
    // [2] (0..5) does NOT overlap [1] (5..15)? Touch at x=5 → overlap (boundary).
    // Use clearer disjoint bbox to verify "[0] is no longer in flight".
    std::vector<Vdp1Cmd> cmds = {
        polyWithBbox( 0,  0,  10, 10),
        polyWithBbox( 5,  5,  15, 15),  // overlaps [0] → barrier
        polyWithBbox(20, 20,  30, 30),  // disjoint from [1] → no barrier
        polyWithBbox( 0,  0,   3,  3),  // would overlap [0] but [0] was barriered out
                                         //  vs [1]: 3<5 → no overlap
                                         //  vs [2]: 3<20 → no overlap
                                         // → no barrier (in-flight = {[1], [2]})
    };
    std::vector<uint32_t> indices = {0, 1, 2, 3};
    auto mask = vdp1c::computeBarrierMask(cmds, indices);
    EXPECT_EQ(mask[0], 0u);
    EXPECT_EQ(mask[1], 1u);
    EXPECT_EQ(mask[2], 0u);
    EXPECT_EQ(mask[3], 0u);
}

// =============================================================================
// pipelineVariantForCmd (Phase A3: cm-mode shader specialization)
// =============================================================================

TEST(PipelineVariant, PolygonReturnsVariantPolygon) {
    Vdp1Cmd c{};
    c.cmdType = VDP1C_TYPE_POLYGON;
    c.pmod = 0u;  // pmod ignored for POLYGON
    EXPECT_EQ(vdp1c::pipelineVariantForCmd(c), vdp1c::VDP1C_VARIANT_POLYGON);
}

TEST(PipelineVariant, DistortedSpriteCmBitsMapToVariants) {
    // cm bits are CMDPMOD bits 3..5 (= (pmod >> 3) & 7)
    for (uint32_t cm = 0; cm <= 5; ++cm) {
        Vdp1Cmd c{};
        c.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
        c.pmod = cm << 3;
        EXPECT_EQ(vdp1c::pipelineVariantForCmd(c),
                  vdp1c::VDP1C_VARIANT_SPRITE_CM0 + cm)
            << "cm=" << cm;
    }
}

TEST(PipelineVariant, ReservedCmReturnsInvalid) {
    // cm=6 / cm=7 are reserved by Saturn spec — skip these draws.
    for (uint32_t cm : {6u, 7u}) {
        Vdp1Cmd c{};
        c.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
        c.pmod = cm << 3;
        EXPECT_EQ(vdp1c::pipelineVariantForCmd(c), vdp1c::VDP1C_VARIANT_INVALID)
            << "cm=" << cm;
    }
}

TEST(PipelineVariant, NonRasterizableTypesReturnInvalid) {
    for (uint32_t t : {VDP1C_TYPE_NOOP, VDP1C_TYPE_NORMAL_SPRITE,
                       VDP1C_TYPE_SCALED_SPRITE, VDP1C_TYPE_POLYLINE,
                       VDP1C_TYPE_LINE}) {
        Vdp1Cmd c{};
        c.cmdType = t;
        c.pmod = 0;
        EXPECT_EQ(vdp1c::pipelineVariantForCmd(c), vdp1c::VDP1C_VARIANT_INVALID)
            << "type=" << t;
    }
}

TEST(PipelineVariant, OtherPmodBitsAreIgnored) {
    // Only bits 3..5 of pmod participate in the variant selection — bits like
    // SPD/ECD/MSB shouldn't shift it.
    Vdp1Cmd c{};
    c.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    c.pmod = (1u << 3) | 0xFFFFFFC7u;  // cm=1, all other bits set
    EXPECT_EQ(vdp1c::pipelineVariantForCmd(c), vdp1c::VDP1C_VARIANT_SPRITE_CM1);
}

TEST(BarrierMask, BarrierCountDropsForMostlyDisjointScene) {
    // Realistic mix: 100 cmds, 90 disjoint + 10 overlapping the prior cmd.
    // Expect barriers = ~10 (one per overlap).
    std::vector<Vdp1Cmd> cmds;
    cmds.reserve(100);
    std::vector<uint32_t> indices;
    indices.reserve(100);
    for (int i = 0; i < 100; ++i) {
        if (i > 0 && (i % 10) == 0) {
            // Every 10th cmd overlaps the prior one.
            cmds.push_back(polyWithBbox((i - 1) * 100, 0, (i - 1) * 100 + 80, 50));
        } else {
            cmds.push_back(polyWithBbox(i * 100, 0, i * 100 + 50, 50));
        }
        indices.push_back(static_cast<uint32_t>(i));
    }
    auto mask = vdp1c::computeBarrierMask(cmds, indices);
    // Each overlap-with-prior at i=10,20,...,90 forces a barrier (9 events).
    EXPECT_EQ(countBarriers(mask), 9);
}
