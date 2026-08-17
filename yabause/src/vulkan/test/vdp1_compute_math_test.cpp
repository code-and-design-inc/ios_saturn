#include "gtest/gtest.h"
#include "../Vdp1ComputeMath.h"
#include <cmath>

using namespace vdp1c;
using glm::vec2;
using glm::ivec2;
using glm::ivec4;

TEST(Vdp1ComputeMath, BBoxCoversAllVertices) {
    auto bbox = computeBBox(
        ivec2(10, 20), ivec2(50, 30), ivec2(60, 80), ivec2(15, 70)
    );
    EXPECT_EQ(bbox.x, 10);
    EXPECT_EQ(bbox.y, 20);
    EXPECT_EQ(bbox.z, 60);
    EXPECT_EQ(bbox.w, 80);
}

TEST(Vdp1ComputeMath, EdgeFunctionLeftSideIsPositive) {
    EXPECT_GT(edgeFunction(vec2(5, 5), vec2(0, 0), vec2(10, 0)), 0.0f);
    EXPECT_LT(edgeFunction(vec2(5, -5), vec2(0, 0), vec2(10, 0)), 0.0f);
}

TEST(Vdp1ComputeMath, InsideQuadAxisAlignedRectangle) {
    vec2 a(0, 0), b(10, 0), c(10, 10), d(0, 10);
    ASSERT_GT(edgeFunction(c, a, b), 0.0f);
    EXPECT_TRUE(insideQuad(vec2(5, 5), a, b, c, d));
    EXPECT_FALSE(insideQuad(vec2(15, 5), a, b, c, d));
    EXPECT_FALSE(insideQuad(vec2(-1, 5), a, b, c, d));
    EXPECT_TRUE(insideQuad(vec2(0, 0), a, b, c, d));
}

TEST(Vdp1ComputeMath, InsideQuadDistortedSEGARallyCase) {
    // SEGA Rally で出るような細いポリゴン (シーム再現の対象)。
    // この座標は既に CCW なので normalizeWinding は no-op になる。
    // 三角形分割すると対角線で誤差が出るが、quad 単一判定なら出ない。
    vec2 a(100, 100), b(105, 102), c(108, 200), d(102, 198);
    bool wasCCW = normalizeWinding(a, b, c, d);
    EXPECT_TRUE(wasCCW);  // CCW 入力確認
    EXPECT_TRUE(insideQuad(vec2(103, 150), a, b, c, d));
    EXPECT_TRUE(insideQuad(vec2(104, 151), a, b, c, d));
}

TEST(Vdp1ComputeMath, InsideQuadCWInputAfterNormalize) {
    // CW 入力 (時計回り、画面座標下向き Y で右回り = 数学的 CW)
    // a=UL, b=LL, c=LR, d=UR → CW
    vec2 a(0, 0), b(0, 10), c(10, 10), d(10, 0);
    ASSERT_LT(edgeFunction(c, a, b), 0.0f);  // CW 確認

    bool wasCCW = normalizeWinding(a, b, c, d);
    EXPECT_FALSE(wasCCW);
    // normalize 後は CCW で内側判定が成立する
    EXPECT_TRUE(insideQuad(vec2(5, 5), a, b, c, d));
}

TEST(Vdp1ComputeMath, IsConcaveDetectsConvexAxisAlignedQuad) {
    // CCW axis-aligned rectangle. All cross products should be the same sign.
    EXPECT_FALSE(isConcave(vec2(0, 0), vec2(10, 0), vec2(10, 10), vec2(0, 10)));
    // CW input — also convex (mixed sign would flip but uniformly).
    EXPECT_FALSE(isConcave(vec2(0, 0), vec2(0, 10), vec2(10, 10), vec2(10, 0)));
}

TEST(Vdp1ComputeMath, IsConcaveDetectsTrueConcaveQuad) {
    // C is pulled strictly inside triangle ABD (the (5,5) midpoint of BD
    // would be a degenerate collinear case, so we use (5,3) which is below
    // the BD line). The interior angle at C exceeds 180° → concave.
    EXPECT_TRUE(isConcave(vec2(0, 0), vec2(10, 0), vec2(5, 3), vec2(0, 10)));
}

TEST(Vdp1ComputeMath, IsConcaveDetectsTwistedQuad) {
    // Self-intersecting "bowtie" quad. Cross products will alternate sign.
    EXPECT_TRUE(isConcave(vec2(0, 0), vec2(10, 10), vec2(10, 0), vec2(0, 10)));
}

TEST(Vdp1ComputeMath, IsConcaveAcceptsParallelogram) {
    // Sheared but convex; should not flag as concave.
    EXPECT_FALSE(isConcave(vec2(0, 0), vec2(10, 2), vec2(12, 12), vec2(2, 10)));
}

TEST(Vdp1ComputeMath, IsConcaveTreatsCollinearEdgePairAsConvex) {
    // Three collinear points with the 4th forming a triangle. Strict-sign
    // gating with epsilon must NOT flag this — degenerate but not concave.
    EXPECT_FALSE(isConcave(vec2(0, 0), vec2(5, 0), vec2(10, 0), vec2(5, 5)));
}

TEST(Vdp1ComputeMath, NormalizeWindingFlipsCWToCCW) {
    vec2 a(0, 0), b(0, 10), c(10, 10), d(10, 0);
    bool wasCCW = normalizeWinding(a, b, c, d);
    EXPECT_FALSE(wasCCW);
    EXPECT_GT(edgeFunction(c, a, b), 0.0f);
}

TEST(Vdp1ComputeMath, EncodePolygonFillsStruct) {
    auto cmd = encodePolygon(
        {{ ivec2(10, 20), ivec2(50, 30), ivec2(60, 80), ivec2(15, 70) }},
        ivec2(0, 0),
        0x7C00,
        0x00C0
    );
    EXPECT_EQ(cmd.cmdType, VDP1C_TYPE_POLYGON);
    EXPECT_EQ(cmd.color, 0x7C00u);
    EXPECT_EQ(cmd.pmod, 0x00C0u);
    EXPECT_EQ(cmd.v0.x, 10);
    EXPECT_EQ(cmd.bbox.x, 10);
    EXPECT_EQ(cmd.bbox.z, 60);
    EXPECT_EQ(cmd.clipMode, 0u);
}

TEST(Vdp1ComputeMath, EncodePolygonAppliesLocalCoord) {
    auto cmd = encodePolygon(
        {{ ivec2(0, 0), ivec2(10, 0), ivec2(10, 10), ivec2(0, 10) }},
        ivec2(160, 112),
        0,
        0
    );
    EXPECT_EQ(cmd.v0.x, 160);
    EXPECT_EQ(cmd.v0.y, 112);
    EXPECT_EQ(cmd.bbox.x, 160);
    EXPECT_EQ(cmd.bbox.z, 170);
}

TEST(Vdp1ComputeMath, EncodePolygonPreservesVertexOrder) {
    // The scanline rasterizer (vidsoft-faithful) walks TL->BL and TR->BR
    // assuming the Saturn standard order v0=TL, v1=TR, v2=BR, v3=BL.
    // Different vertex orderings render as texture rotations, so we do NOT
    // apply CCW normalization (B<->D swap).
    // Input: a=UL, b=LL, c=LR, d=UR (visually clockwise on screen).
    auto cmd = encodePolygon(
        {{ ivec2(0, 0), ivec2(0, 10), ivec2(10, 10), ivec2(10, 0) }},
        ivec2(0, 0),
        0,
        0
    );
    // The input order must be preserved verbatim.
    EXPECT_EQ(cmd.v0, ivec2(0, 0));
    EXPECT_EQ(cmd.v1, ivec2(0, 10));
    EXPECT_EQ(cmd.v2, ivec2(10, 10));
    EXPECT_EQ(cmd.v3, ivec2(10, 0));
    // bbox covers all 4 vertices.
    EXPECT_EQ(cmd.bbox.x, 0);
    EXPECT_EQ(cmd.bbox.y, 0);
    EXPECT_EQ(cmd.bbox.z, 10);
    EXPECT_EQ(cmd.bbox.w, 10);
}

TEST(Vdp1State, ResetReturnsDefaultValues) {
    vdp1c::Vdp1State s;
    vdp1c::resetState(s);
    EXPECT_EQ(s.systemClip, glm::ivec2(2047, 2047));
    EXPECT_EQ(s.userClip,   glm::ivec4(0, 0, 2047, 2047));
    EXPECT_EQ(s.localCoord, glm::ivec2(0, 0));
}

TEST(Vdp1State, ApplyStateOverwritesCmdFields) {
    Vdp1State s;
    s.systemClip = glm::ivec2(500, 400);
    s.userClip   = glm::ivec4(10, 20, 300, 250);
    s.localCoord = glm::ivec2(50, 60);

    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_POLYGON;
    cmd.v0 = glm::ivec2(0, 0);
    cmd.v1 = glm::ivec2(10, 0);
    cmd.v2 = glm::ivec2(10, 10);
    cmd.v3 = glm::ivec2(0, 10);

    applyState(cmd, s);
    EXPECT_EQ(cmd.systemClip, glm::ivec4(500, 400, 0, 0));
    EXPECT_EQ(cmd.userClip,   glm::ivec4(10, 20, 300, 250));
    EXPECT_EQ(cmd.v0, glm::ivec2(50, 60));
    EXPECT_EQ(cmd.v1, glm::ivec2(60, 60));
    EXPECT_EQ(cmd.v2, glm::ivec2(60, 70));
    EXPECT_EQ(cmd.v3, glm::ivec2(50, 70));
    EXPECT_EQ(cmd.bbox, glm::ivec4(50, 60, 60, 70));
}

TEST(Vdp1State, ApplyStateSetsFlagThinForNarrowPolygon) {
    // Axis-aligned 100x1 thin polygon at scaleMax=1.0 → FLAG_THIN set.
    Vdp1State s;
    s.scaleMax = 1.0f;
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_POLYGON;
    cmd.v0 = glm::ivec2(0, 0);
    cmd.v1 = glm::ivec2(100, 0);
    cmd.v2 = glm::ivec2(100, 1);
    cmd.v3 = glm::ivec2(0, 1);
    applyState(cmd, s);
    EXPECT_NE(cmd.flags & VDP1C_FLAG_THIN, 0u);
}

TEST(Vdp1State, ApplyStateClearsFlagThinForUnitDot) {
    // FLAG_THIN は HD upscale で「線が台形に化けて隙間が並ぶ」現象の補正用。
    // 1×1 のドットは長軸が無いので隙間が形成されようがなく、THIN にしては
    // いけない (旧実装ではドットも常に THIN になっていた)。
    Vdp1State s;
    s.scaleMax = 1.0f;
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_POLYGON;
    cmd.v0 = glm::ivec2(0, 0);
    cmd.v1 = glm::ivec2(1, 0);
    cmd.v2 = glm::ivec2(1, 1);
    cmd.v3 = glm::ivec2(0, 1);
    applyState(cmd, s);
    EXPECT_EQ(cmd.flags & VDP1C_FLAG_THIN, 0u);
}

TEST(Vdp1State, ApplyStateClearsFlagThinForShortNarrowPolygon) {
    // 1×3 の短片: thickness=1 で薄いが、aspect=3 では隙間が並ぶ長軸がない。
    // 長辺/短辺比が 4 未満なら THIN にしない、という新ゲートの境界テスト。
    Vdp1State s;
    s.scaleMax = 1.0f;
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_POLYGON;
    cmd.v0 = glm::ivec2(0, 0);
    cmd.v1 = glm::ivec2(3, 0);
    cmd.v2 = glm::ivec2(3, 1);
    cmd.v3 = glm::ivec2(0, 1);
    applyState(cmd, s);
    EXPECT_EQ(cmd.flags & VDP1C_FLAG_THIN, 0u);
}

TEST(Vdp1State, ApplyStateClearsFlagThinForNormalPolygon) {
    Vdp1State s;
    s.scaleMax = 1.0f;
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    cmd.v0 = glm::ivec2(0, 0);
    cmd.v1 = glm::ivec2(24, 0);
    cmd.v2 = glm::ivec2(24, 24);
    cmd.v3 = glm::ivec2(0, 24);
    applyState(cmd, s);
    EXPECT_EQ(cmd.flags & VDP1C_FLAG_THIN, 0u);
}

TEST(Vdp1State, ApplyStateSetsFlagThinForDiagonalNarrowPolygon) {
    // bbox-only would miss this case; geometric thickness=1 catches it.
    Vdp1State s;
    s.scaleMax = 1.0f;
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    cmd.v0 = glm::ivec2(0,  0);
    cmd.v1 = glm::ivec2(70, 70);
    cmd.v2 = glm::ivec2(69, 71);
    cmd.v3 = glm::ivec2(-1, 1);
    applyState(cmd, s);
    EXPECT_NE(cmd.flags & VDP1C_FLAG_THIN, 0u);
}

TEST(Vdp1ComputeMath, IsThinPolygonGeometricOnSliverDump) {
    // 実シーン JSONL dump からのスライバー quad 群 (Δy=1 の傾いた細片で
    // 隣接 polygon 間の隙間源となるパターン)。新アスペクト比ゲート
    // (longest/thickness ≥ 4) で全件 THIN 判定される必要がある。
    // applyState 側の FLAG_THIN ゲートは隙間調査中に一時無効化されている
    // ことがあるので、算法そのものを isThinPolygonGeometric に直接ぶつけて
    // テストする (applyState 経由ではなく)。
    constexpr float scaleMax = 1.0f;

    struct SliverCase {
        const char* name;
        glm::ivec2 v0, v1, v2, v3;
    };
    const SliverCase cases[] = {
        {"#01", {  0,-1}, {-22,-2}, {-26,-1}, { -2,-1}}, // aspect ≈ 44
        {"#02", {-18,-2}, {-38,-3}, {-44,-2}, {-22,-2}}, // aspect ≈ 37
        {"#03 v0==v3 deg", { 22,-2}, {  3,-3}, {  1,-2}, { 22,-2}}, // aspect ≈ 42
        {"#04", { 57,-2}, { 39,-3}, { 40,-2}, { 60,-2}}, // aspect ≈ 47
        {"#05", {  7,-3}, {-10,-4}, {-13,-3}, {  5,-3}}, // aspect ≈ 32
        {"#06", { 53,-3}, { 37,-4}, { 38,-3}, { 54,-3}}, // aspect ≈ 34
        {"#07", { -3,-4}, {-12,-5}, {-16,-4}, { -5,-4}}, // aspect ≈ 19
        {"#08", { 10,-4}, { -2,-5}, { -3,-4}, {  9,-4}}, // aspect ≈ 22
        {"#09", { 12,-4}, {  1,-5}, { -2,-5}, { 10,-4}}, // aspect ≈ 58
        {"#10", { 26,-4}, { 14,-5}, { 12,-4}, { 25,-4}}, // aspect ≈ 24
        {"#11", { 28,-4}, { 17,-5}, { 14,-5}, { 26,-4}}, // aspect ≈ 58
        {"#12", { 30,-4}, { 19,-5}, { 17,-5}, { 28,-4}}, // aspect ≈ 61
        {"#13", { 42,-4}, { 32,-5}, { 30,-4}, { 41,-4}}, // aspect ≈ 20
        {"#14", { 43,-4}, { 33,-5}, { 32,-5}, { 42,-4}}, // aspect ≈ 101
        {"#15", { 44,-4}, { 35,-5}, { 33,-5}, { 43,-4}}, // aspect ≈ 67
        {"#16", { 45,-4}, { 36,-5}, { 35,-5}, { 44,-4}}, // aspect ≈ 82
        {"#17 shortest", { 25,-6}, { 29,-5}, { 31,-5}, { 28,-6}}, // aspect ≈ 6.8 (境界寄り)
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);
        Vdp1Cmd cmd{};
        cmd.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
        cmd.v0 = c.v0;
        cmd.v1 = c.v1;
        cmd.v2 = c.v2;
        cmd.v3 = c.v3;
        EXPECT_TRUE(isThinPolygonGeometric(cmd, scaleMax))
            << "Sliver case " << c.name << " must be classified THIN "
            << "(隣接 polygon 隙間補償が必要)";
    }
}

TEST(Vdp1ComputeMath, IsThinPolygonGeometricNotThinOnSmallChunkDump) {
    // 実シーン JSONL dump からの小チャンク polygon 群。bbox が両方向とも
    // 数 px しかない / 退化 (3 verts 重複) / 帯状になっていない、のいずれ
    // かで隙間源にはならないため NOT thin であるべき。
    // 旧 aspect-only ゲートでは 5/10 が誤って THIN 判定されていたので
    // 回帰防止用に固定。
    constexpr float scaleMax = 1.0f;

    struct ChunkCase {
        const char* name;
        glm::ivec2 v0, v1, v2, v3;
    };
    const ChunkCase cases[] = {
        // #1 v1==v2==v3 → 線分相当, area=0 (degenerate guard で弾く)
        {"#1 deg-line", {-86,24}, {-81,23}, {-81,23}, {-81,23}},
        // #2 v2==v3 → 三角形, bbox 7x3, fill=0.19
        {"#2 tri",      {-74,26}, {-76,24}, {-81,23}, {-81,23}},
        // #3 ねじれ, bbox 12x4, bboxShort=4 > widthThr=3
        {"#3 twist",    {-85,38}, {-81,36}, {-91,38}, {-93,40}},
        // #4 bbox 11x4, bboxShort=4 > 3
        {"#4 chunk",    {-75,35}, {-77,38}, {-67,39}, {-66,38}},
        // #5 bbox 2x7, aspect=3.5 < 4
        {"#5 short",    {-82,41}, {-82,37}, {-84,34}, {-83,40}},
        // #6 bbox 3x7, aspect=2.33 < 4
        {"#6 short",    {-68,42}, {-67,48}, {-65,46}, {-67,41}},
        // #7 bbox 3x3 ほぼ正方形
        {"#7 square",   {-80,32}, {-77,31}, {-77,29}, {-80,30}},
        // #8 v2==v3, bbox 2x2 小チャンク
        {"#8 small",    {-80,35}, {-81,34}, {-82,36}, {-82,36}},
        // #9 v2==v3, bbox 8x3, fill=0.29 (帯状ではない)
        {"#9 tri-mid",  {-85,27}, {-80,30}, {-77,29}, {-77,29}},
        // #10 bbox 3x2 小チャンク
        {"#10 small",   {-83,31}, {-80,32}, {-80,30}, {-83,30}},
    };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);
        Vdp1Cmd cmd{};
        cmd.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
        cmd.v0 = c.v0;
        cmd.v1 = c.v1;
        cmd.v2 = c.v2;
        cmd.v3 = c.v3;
        EXPECT_FALSE(isThinPolygonGeometric(cmd, scaleMax))
            << "Chunk case " << c.name << " must NOT be classified THIN "
            << "(隙間が出る帯状ではない / 退化 / 短い小チャンク)";
    }
}

TEST(Vdp1State, ApplyStateFlagsThinAndTriSplitOnDegenerateThin) {
    // Real cmd 324 from JSONL: twisted-thin polygon with 3 verts on
    // y=471 + 1 vertex offset to y=467. isConcave detects sign flip
    // → TRI_SPLIT also set, but FLAG_THIN must also be set so the
    // shader routes through supercover (THIN priority over TRI_SPLIT).
    Vdp1State s;
    s.scaleMax = 4.0f;  // RES_NATIVE-ish HD scale
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    cmd.v0 = glm::ivec2(1014, 471);
    cmd.v1 = glm::ivec2(925, 467);
    cmd.v2 = glm::ivec2(937, 471);
    cmd.v3 = glm::ivec2(1031, 471);
    applyState(cmd, s);
    EXPECT_NE(cmd.flags & VDP1C_FLAG_THIN, 0u)
        << "cmd 324 must be classified thin so the shader applies supercover";
}

TEST(InverseBilinear, ParallelogramReturnsLinearMapping) {
    // 軸整合 10x10 単位四角形、中心点
    glm::vec2 A(0,0), B(10,0), C(10,10), D(0,10);
    auto st = inverseBilinear(glm::vec2(5, 5), A, B, C, D);
    EXPECT_NEAR(st.x, 0.5f, 1e-3f);
    EXPECT_NEAR(st.y, 0.5f, 1e-3f);
}

TEST(InverseBilinear, ParallelogramCornerA) {
    glm::vec2 A(0,0), B(10,0), C(10,10), D(0,10);
    auto st = inverseBilinear(A, A, B, C, D);
    EXPECT_NEAR(st.x, 0.0f, 1e-3f);
    EXPECT_NEAR(st.y, 0.0f, 1e-3f);
}

TEST(InverseBilinear, ParallelogramCornerC) {
    glm::vec2 A(0,0), B(10,0), C(10,10), D(0,10);
    auto st = inverseBilinear(C, A, B, C, D);
    EXPECT_NEAR(st.x, 1.0f, 1e-3f);
    EXPECT_NEAR(st.y, 1.0f, 1e-3f);
}

TEST(InverseBilinear, DistortedQuadInteriorWithinUnit) {
    // SEGA Rally 風の細い変形 quad、内部点が (s,t) ∈ [0,1]^2 に収まる
    glm::vec2 A(100,100), B(105,102), C(108,200), D(102,198);
    auto st = inverseBilinear(glm::vec2(104, 150), A, B, C, D);
    EXPECT_GE(st.x, 0.0f); EXPECT_LE(st.x, 1.0f);
    EXPECT_GE(st.y, 0.0f); EXPECT_LE(st.y, 1.0f);
}

// Degenerate quad with v2==v3 (Saturn distorted-sprite triangle encoding).
// Forward mapping: P = mix(mix(v0, v1, s), v2, t) collapses bottom row at v2.
// Inverse must pick the strictly interior root, NOT the collapsed t=1 root,
// otherwise s falls back to 0 and texture content shifts.
//
// Real cmd from yabause user report (2026-05-12):
//   v0=(1610,343), v1=(1604,311), v2=v3=(1598,317).
// Centroid of triangle = (1604, 323.667). Solving forward equation gives
// (s, t) = (0.5, 1/3).
TEST(InverseBilinear, DegenerateQuadV2EqualsV3CWWindingCentroid) {
    glm::vec2 A(1610.f, 343.f);
    glm::vec2 B(1604.f, 311.f);
    glm::vec2 C(1598.f, 317.f);
    glm::vec2 D(1598.f, 317.f);  // v3 == v2
    glm::vec2 P(1604.f, 323.6667f);  // centroid of triangle ABC
    auto st = inverseBilinear(P, A, B, C, D);
    EXPECT_NEAR(st.x, 0.5f,       1e-2f);
    EXPECT_NEAR(st.y, 1.0f / 3.0f, 1e-2f);
}

// Same shape, mirrored to give CCW winding. The two roots t0/t1 swap order
// in the quadratic solver, so the tie-break must handle both windings.
TEST(InverseBilinear, DegenerateQuadV2EqualsV3CCWWindingCentroid) {
    // Mirror across y axis to flip winding from CW to CCW.
    glm::vec2 A(-1610.f, 343.f);
    glm::vec2 B(-1604.f, 311.f);
    glm::vec2 C(-1598.f, 317.f);
    glm::vec2 D(-1598.f, 317.f);
    glm::vec2 P(-1604.f, 323.6667f);
    auto st = inverseBilinear(P, A, B, C, D);
    EXPECT_NEAR(st.x, 0.5f,       1e-2f);
    EXPECT_NEAR(st.y, 1.0f / 3.0f, 1e-2f);
}

// Midpoint of v1-v2 edge: (s, t) = (1, 0.5) per forward bilinear math.
TEST(InverseBilinear, DegenerateQuadV2EqualsV3MidpointOfV1V2Edge) {
    glm::vec2 A(1610.f, 343.f);
    glm::vec2 B(1604.f, 311.f);
    glm::vec2 C(1598.f, 317.f);
    glm::vec2 D(1598.f, 317.f);
    glm::vec2 P = 0.5f * B + 0.5f * C;  // = (1601, 314)
    auto st = inverseBilinear(P, A, B, C, D);
    EXPECT_NEAR(st.x, 1.0f, 1e-2f);
    EXPECT_NEAR(st.y, 0.5f, 1e-2f);
}

// Midpoint of v0-v2 edge: (s, t) = (0, 0.5) per forward bilinear math.
TEST(InverseBilinear, DegenerateQuadV2EqualsV3MidpointOfV0V2Edge) {
    glm::vec2 A(1610.f, 343.f);
    glm::vec2 B(1604.f, 311.f);
    glm::vec2 C(1598.f, 317.f);
    glm::vec2 D(1598.f, 317.f);
    glm::vec2 P = 0.5f * A + 0.5f * C;  // = (1604, 330)
    auto st = inverseBilinear(P, A, B, C, D);
    EXPECT_NEAR(st.x, 0.0f, 1e-2f);
    EXPECT_NEAR(st.y, 0.5f, 1e-2f);
}

TEST(EncodeDistortedSprite, FieldsArePopulated) {
    std::array<glm::ivec2, 4> verts = {{
        glm::ivec2(10, 20), glm::ivec2(50, 25),
        glm::ivec2(55, 80), glm::ivec2(15, 70)
    }};
    auto cmd = encodeDistortedSprite(
        verts,
        /*srca*/ 0x1234,
        /*charSize*/ glm::uvec2(32, 24),
        /*color*/ 0x8000,
        /*pmod*/ 0x0028,  // cm=5 (16bpp RGB direct)
        /*localCoord*/ glm::ivec2(0, 0));
    EXPECT_EQ(cmd.cmdType, VDP1C_TYPE_DISTORTED_SPRITE);
    EXPECT_EQ(cmd.srca,    0x1234u);
    EXPECT_EQ(cmd.charSize, glm::uvec2(32, 24));
    EXPECT_EQ(cmd.pmod,    0x0028u);
    EXPECT_EQ(cmd.color,   0x8000u);
    EXPECT_EQ(cmd.bbox,    glm::ivec4(10, 20, 55, 80));
}

TEST(EncodeDistortedSprite, LocalCoordIsApplied) {
    std::array<glm::ivec2, 4> verts = {{
        glm::ivec2(0, 0), glm::ivec2(10, 0),
        glm::ivec2(10, 10), glm::ivec2(0, 10)
    }};
    auto cmd = encodeDistortedSprite(verts, 0, glm::uvec2(0), 0, 0, glm::ivec2(100, 200));
    EXPECT_EQ(cmd.v0, glm::ivec2(100, 200));
    EXPECT_EQ(cmd.v2, glm::ivec2(110, 210));
    EXPECT_EQ(cmd.bbox, glm::ivec4(100, 200, 110, 210));
}

// =============================================================================
// Thin-polygon classification (FLAG_THIN gate)
// =============================================================================

TEST(ThinPolygon, IsThinPolygonGeometricMatchesAxisAlignedThin) {
    // 100 x 1 axis-aligned rectangle: area=100, longest=100 → thickness=1.
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_POLYGON;
    cmd.v0 = glm::ivec2(0, 0);
    cmd.v1 = glm::ivec2(100, 0);
    cmd.v2 = glm::ivec2(100, 1);
    cmd.v3 = glm::ivec2(0, 1);
    EXPECT_TRUE(isThinPolygonGeometric(cmd, 1.0f));  // 1 ≤ 3
}

TEST(ThinPolygon, IsThinPolygonGeometricCatchesDiagonalThin) {
    // 100 px diagonal at 45°, 1 px perpendicular width. bbox は ~71x71 で
    // bbox-aspect 軸では検出できないが、polygon area が bbox area の 3% 程度
    // しかないので fill-ratio ルート (rule (b)) で THIN 判定される。
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    cmd.v0 = glm::ivec2(0,  0);
    cmd.v1 = glm::ivec2(70, 70);
    cmd.v2 = glm::ivec2(70 - 1, 70 + 1);
    cmd.v3 = glm::ivec2(0  - 1,  0 + 1);
    cmd.bbox = glm::ivec4(-1, 0, 70, 71);
    EXPECT_TRUE(isThinPolygonGeometric(cmd, 1.0f));
}

TEST(ThinPolygon, IsThinPolygonGeometricRejectsNormalSprite) {
    // 24x24 normal sprite: area=576, longest=24 → thickness=24.
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    cmd.v0 = glm::ivec2(0,  0);
    cmd.v1 = glm::ivec2(24, 0);
    cmd.v2 = glm::ivec2(24, 24);
    cmd.v3 = glm::ivec2(0,  24);
    EXPECT_FALSE(isThinPolygonGeometric(cmd, 1.0f));  // 24 > 3
    EXPECT_FALSE(isThinPolygonGeometric(cmd, 4.0f));  // 24 > 9
}

TEST(ThinPolygon, IsThinPolygonGeometricHonorsScaleMaxThreshold) {
    // 50x6 rectangle: area=300, longest=50 → thickness=6.
    //   scaleMax=4.0 → thr=9: thin
    //   scaleMax=1.0 → thr=3: not thin
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_POLYGON;
    cmd.v0 = glm::ivec2(0, 0);
    cmd.v1 = glm::ivec2(50, 0);
    cmd.v2 = glm::ivec2(50, 6);
    cmd.v3 = glm::ivec2(0, 6);
    EXPECT_TRUE(isThinPolygonGeometric(cmd, 4.0f));
    EXPECT_FALSE(isThinPolygonGeometric(cmd, 1.0f));
}

TEST(ThinPolygon, IsThinPolygonGeometricCatchesTriangularSlivers) {
    // Real cmd 332 from JSONL: bbox 110x4, vertex layout is
    //   v0=(747,471) v1=(654,467) v2=(637,471) v3=(739,471)
    // = 3 verts on bottom edge (y=471) + 1 vertex offset upward (v1 at y=467).
    // HD upscale (scaleMax=4 → widthThr=9) では bbox 短辺 4 ≤ 9 + aspect
    // 110/4=27.5 ≥ 4 で THIN 判定。Saturn 等倍 (scaleMax=1 → widthThr=3)
    // では bbox 短辺 4 > 3 のため隙間が並ぶリスクがなく NOT thin。
    Vdp1Cmd cmd{};
    cmd.cmdType = VDP1C_TYPE_DISTORTED_SPRITE;
    cmd.v0 = glm::ivec2(747, 471);
    cmd.v1 = glm::ivec2(654, 467);
    cmd.v2 = glm::ivec2(637, 471);
    cmd.v3 = glm::ivec2(739, 471);
    EXPECT_TRUE(isThinPolygonGeometric(cmd, 4.0f));   // HD: bbox 短辺 4 ≤ 9
    EXPECT_FALSE(isThinPolygonGeometric(cmd, 1.0f));  // 1×: bbox 短辺 4 > 3
}


TEST(ThinPolygon, ResetStatePreservesPersistentState) {
    // scaleMax / localCoord / systemClip / userClip は VDP1 の persistent
    // state。フレーム頭の resetState (beginFrame() 経由) ではリセット
    // されず、ゲームが LocalCoord / SystemClip / UserClip cmd を
    // 再送するまで前 frame の値を保持する (Saturn HW 挙動 + graphics
    // path との整合)。
    Vdp1State s;
    s.scaleMax   = 3.77f;
    s.localCoord = glm::ivec2(176, 112);
    s.systemClip = glm::ivec2(319, 223);
    s.userClip   = glm::ivec4(10, 20, 300, 200);
    resetState(s);
    EXPECT_FLOAT_EQ(s.scaleMax, 3.77f);
    EXPECT_EQ(s.localCoord, glm::ivec2(176, 112));
    EXPECT_EQ(s.systemClip, glm::ivec2(319, 223));
    EXPECT_EQ(s.userClip,   glm::ivec4(10, 20, 300, 200));
}
