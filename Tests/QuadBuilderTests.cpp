/*
    Created on: 10/4/26
    Filename: QuadBuilderTests.cpp
    Description: Quad geometry, UV and colour tests for QuadBuilder.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/QuadBuilder/QuadBuilder.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

namespace {

    constexpr gfx::Vec2 Sheet{ 4096.0f, 512.0f };
    constexpr size_t TL = 0;
    constexpr size_t TR = 1;
    constexpr size_t BR = 2;
    constexpr size_t BL = 3;

    gfx::SpriteDraw sprite(const gfx::Rect dest, const gfx::Rect source)
    {
        gfx::SpriteDraw draw;
        draw.texture = static_cast<gfx::TextureHandle>(1);
        draw.dest = dest;
        draw.source = source;

        return draw;
    }

}

TEST(QuadBuilder, AxisAlignedCornersFollowDest)
{
    const gfx::Quad q = gfx::buildQuad(sprite({ 10.0f, 20.0f, 30.0f, 40.0f }, {}), Sheet);

    EXPECT_FLOAT_EQ(q[TL].x, 10.0f); EXPECT_FLOAT_EQ(q[TL].y, 20.0f);
    EXPECT_FLOAT_EQ(q[TR].x, 40.0f); EXPECT_FLOAT_EQ(q[TR].y, 20.0f);
    EXPECT_FLOAT_EQ(q[BR].x, 40.0f); EXPECT_FLOAT_EQ(q[BR].y, 60.0f);
    EXPECT_FLOAT_EQ(q[BL].x, 10.0f); EXPECT_FLOAT_EQ(q[BL].y, 60.0f);
}

TEST(QuadBuilder, SourceRectBecomesNormalisedUv)
{
    const gfx::Quad q = gfx::buildQuad(sprite({ 0.0f, 0.0f, 128.0f, 128.0f }, { 1024.0f, 0.0f, 512.0f, 512.0f }), Sheet);

    EXPECT_FLOAT_EQ(q[TL].u, 0.25f);  EXPECT_FLOAT_EQ(q[TL].v, 0.0f);
    EXPECT_FLOAT_EQ(q[BR].u, 0.375f); EXPECT_FLOAT_EQ(q[BR].v, 1.0f);
    EXPECT_FLOAT_EQ(q[TR].u, 0.375f); EXPECT_FLOAT_EQ(q[BL].u, 0.25f);
}

TEST(QuadBuilder, EmptySourceMeansWholeTexture)
{
    for (const gfx::Rect source : { gfx::Rect{}, gfx::Rect{ 5.0f, 5.0f, -1.0f, 10.0f }, gfx::Rect{ 5.0f, 5.0f, 10.0f, 0.0f } }) {
        const gfx::Quad q = gfx::buildQuad(sprite({ 0.0f, 0.0f, 8.0f, 8.0f }, source), Sheet);

        EXPECT_FLOAT_EQ(q[TL].u, 0.0f); EXPECT_FLOAT_EQ(q[TL].v, 0.0f);
        EXPECT_FLOAT_EQ(q[BR].u, 1.0f); EXPECT_FLOAT_EQ(q[BR].v, 1.0f);
    }
}

TEST(QuadBuilder, FlipXSwapsHorizontalUv)
{
    gfx::SpriteDraw draw = sprite({ 0.0f, 0.0f, 128.0f, 128.0f }, { 1024.0f, 0.0f, 512.0f, 512.0f });
    draw.flipX = true;
    const gfx::Quad q = gfx::buildQuad(draw, Sheet);

    EXPECT_FLOAT_EQ(q[TL].u, 0.375f);
    EXPECT_FLOAT_EQ(q[TR].u, 0.25f);
    EXPECT_FLOAT_EQ(q[TL].x, 0.0f);     // positions are not mirrored, only the texture
    EXPECT_FLOAT_EQ(q[TL].v, 0.0f);
}

TEST(QuadBuilder, TintIsCopiedToEveryCorner)
{
    gfx::SpriteDraw draw = sprite({ 0.0f, 0.0f, 8.0f, 8.0f }, {});
    draw.tint = gfx::Color{ 10, 20, 30, 40 };

    for (const gfx::QuadVertex& v : gfx::buildQuad(draw, Sheet)) {
        EXPECT_EQ(v.r, 10); EXPECT_EQ(v.g, 20); EXPECT_EQ(v.b, 30); EXPECT_EQ(v.a, 40);
    }
}

TEST(QuadBuilder, QuarterTurnIsClockwiseInYDown)
{
    gfx::SpriteDraw draw = sprite({ 0.0f, 0.0f, 10.0f, 10.0f }, {});
    draw.rotation = std::numbers::pi_v<float> * 0.5f;
    const gfx::Quad q = gfx::buildQuad(draw, Sheet);

    EXPECT_NEAR(q[TL].x, 10.0f, 1e-4f); EXPECT_NEAR(q[TL].y, 0.0f, 1e-4f);
    EXPECT_NEAR(q[TR].x, 10.0f, 1e-4f); EXPECT_NEAR(q[TR].y, 10.0f, 1e-4f);
    EXPECT_NEAR(q[BR].x, 0.0f, 1e-4f);  EXPECT_NEAR(q[BR].y, 10.0f, 1e-4f);
    EXPECT_NEAR(q[BL].x, 0.0f, 1e-4f);  EXPECT_NEAR(q[BL].y, 0.0f, 1e-4f);
}

TEST(QuadBuilder, FlipAndRotationCombine)
{
    gfx::SpriteDraw draw = sprite({ 0.0f, 0.0f, 10.0f, 10.0f }, {});
    draw.rotation = std::numbers::pi_v<float> * 0.5f;
    draw.flipX = true;
    const gfx::Quad q = gfx::buildQuad(draw, Sheet);

    EXPECT_NEAR(q[TL].x, 10.0f, 1e-4f); EXPECT_NEAR(q[TL].y, 0.0f, 1e-4f);
    EXPECT_FLOAT_EQ(q[TL].u, 1.0f);
    EXPECT_FLOAT_EQ(q[TR].u, 0.0f);
}

TEST(QuadBuilder, RectUsesFullUvAndItsColor)
{
    const gfx::RectDraw rect{ gfx::Rect{ 1.0f, 2.0f, 3.0f, 4.0f }, gfx::Color{ 9, 8, 7, 6 }, 0, gfx::Space::Screen };
    const gfx::Quad q = gfx::buildQuad(rect);

    EXPECT_FLOAT_EQ(q[TL].x, 1.0f); EXPECT_FLOAT_EQ(q[BR].y, 6.0f);
    EXPECT_FLOAT_EQ(q[TL].u, 0.0f); EXPECT_FLOAT_EQ(q[BR].u, 1.0f);
    EXPECT_FLOAT_EQ(q[TL].v, 0.0f); EXPECT_FLOAT_EQ(q[BR].v, 1.0f);
    for (const gfx::QuadVertex& v : q) {
        EXPECT_EQ(v.r, 9); EXPECT_EQ(v.a, 6);
    }
}

TEST(QuadBuilder, ZeroTextureSizeFallsBackToWholeTexture)
{
    const gfx::Quad q = gfx::buildQuad(sprite({ 0.0f, 0.0f, 8.0f, 8.0f }, { 0.0f, 0.0f, 10.0f, 10.0f }), gfx::Vec2{ 0.0f, 0.0f });

    for (const gfx::QuadVertex& v : q) {
        EXPECT_TRUE(std::isfinite(v.u) && std::isfinite(v.v));
    }
    EXPECT_FLOAT_EQ(q[TL].u, 0.0f);
    EXPECT_FLOAT_EQ(q[BR].u, 1.0f);
}

TEST(QuadBuilder, IndicesFormTwoTrianglesOverTheCorners)
{
    EXPECT_EQ(gfx::QuadIndices, (std::array<uint16_t, 6>{ 0, 1, 2, 0, 2, 3 }));
}
