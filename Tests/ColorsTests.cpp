/*
    Created on: 11/9/26
    Filename: ColorsTests.cpp
    Description: Tests for the colour helpers.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/Colors/Colors.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <optional>

namespace {

    void expectColor(const gfx::Color color, const int r, const int g, const int b, const int a)
    {
        EXPECT_EQ(color.r, r);
        EXPECT_EQ(color.g, g);
        EXPECT_EQ(color.b, b);
        EXPECT_EQ(color.a, a);
    }

    void expectNear(const gfx::Color actual, const gfx::Color expected)
    {
        EXPECT_NEAR(actual.r, expected.r, 1);
        EXPECT_NEAR(actual.g, expected.g, 1);
        EXPECT_NEAR(actual.b, expected.b, 1);
        EXPECT_NEAR(actual.a, expected.a, 1);
    }

} // namespace

TEST(Colors, FromHexSplitsChannels)
{
    expectColor(gfx::fromHex(0x11223344), 0x11, 0x22, 0x33, 0x44);
}

TEST(Colors, ParseHexReadsBothLengths)
{
    const auto rgb = gfx::parseHex("#112233");
    ASSERT_TRUE(rgb.has_value());
    expectColor(*rgb, 0x11, 0x22, 0x33, 255);

    const auto rgba = gfx::parseHex("#11223344");
    ASSERT_TRUE(rgba.has_value());
    expectColor(*rgba, 0x11, 0x22, 0x33, 0x44);

    const auto mixed = gfx::parseHex("#aaBBcc");
    ASSERT_TRUE(mixed.has_value());
    expectColor(*mixed, 0xAA, 0xBB, 0xCC, 255);
}

TEST(Colors, ParseHexRejectsBadInput)
{
    EXPECT_EQ(gfx::parseHex(""), std::nullopt);
    EXPECT_EQ(gfx::parseHex("112233"), std::nullopt);
    EXPECT_EQ(gfx::parseHex("#12345"), std::nullopt);
    EXPECT_EQ(gfx::parseHex("#12345G"), std::nullopt);
    EXPECT_EQ(gfx::parseHex("#1122334455"), std::nullopt);
}

TEST(Colors, HsvPrimaries)
{
    expectColor(gfx::fromHsv({ 0.0f, 1.0f, 1.0f }), 255, 0, 0, 255);
    expectColor(gfx::fromHsv({ 120.0f, 1.0f, 1.0f }), 0, 255, 0, 255);
    expectColor(gfx::fromHsv({ 240.0f, 1.0f, 1.0f }), 0, 0, 255, 255);
    expectColor(gfx::fromHsv({ 0.0f, 0.0f, 1.0f }), 255, 255, 255, 255);
}

TEST(Colors, HsvWrapsHueAndClamps)
{
    expectColor(gfx::fromHsv({ 360.0f, 1.0f, 1.0f }), 255, 0, 0, 255);
    expectColor(gfx::fromHsv({ -360.0f, 1.0f, 1.0f }), 255, 0, 0, 255);
    expectColor(gfx::fromHsv({ 0.0f, 2.0f, 2.0f, 2.0f }), 255, 0, 0, 255);
}

TEST(Colors, ToHsvRoundTrips)
{
    for (const gfx::Color c : { gfx::Red, gfx::Orange, gfx::Gray, gfx::Cyan }) {
        expectNear(gfx::fromHsv(gfx::toHsv(c)), c);
    }
}

TEST(Colors, ToHsvOfBlackHasZeroSaturation)
{
    const gfx::Hsv hsv = gfx::toHsv(gfx::Black);
    EXPECT_EQ(hsv.s, 0.0f);
    EXPECT_FALSE(std::isnan(hsv.h));
    EXPECT_FALSE(std::isnan(hsv.s));
    EXPECT_FALSE(std::isnan(hsv.v));
}

TEST(Colors, LerpEndsAndClamps)
{
    const gfx::Color a{ 0, 10, 20, 30 };
    const gfx::Color b{ 255, 200, 100, 255 };
    expectColor(gfx::lerp(a, b, 0.0f), 0, 10, 20, 30);
    expectColor(gfx::lerp(a, b, 1.0f), 255, 200, 100, 255);
    expectColor(gfx::lerp(a, b, -1.0f), 0, 10, 20, 30);
    expectColor(gfx::lerp(a, b, 2.0f), 255, 200, 100, 255);

    const int mid = gfx::lerp(a, b, 0.5f).r;
    EXPECT_TRUE(mid == 127 || mid == 128);
}

TEST(Colors, WithAlphaKeepsRgb)
{
    expectColor(gfx::withAlpha(gfx::Orange, 10), 255, 165, 0, 10);
}

TEST(Colors, MultiplyWhiteIsIdentityAndBlackIsZero)
{
    const gfx::Color c{ 12, 34, 56, 78 };
    expectColor(gfx::multiply(c, gfx::White), 12, 34, 56, 78);
    expectColor(gfx::multiply(c, gfx::Black), 0, 0, 0, 78);
    expectColor(gfx::multiply({ 128, 128, 128, 255 }, { 128, 128, 128, 255 }), 64, 64, 64, 255);
}
