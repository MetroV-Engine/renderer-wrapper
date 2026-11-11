/*
    Created on: 10/4/26
    Filename: SpriteAnimationTests.cpp
    Description: Frame timing tests for SpriteAnimation.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/SpriteAnimation/SpriteAnimation.hpp"

#include <gtest/gtest.h>

namespace {

    constexpr gfx::Vec2 Frame{ 512.0f, 512.0f };

    // 4 fps: a frame lasts exactly 0.25 s, which floats represent without rounding.
    constexpr float Fps = 4.0f;
    constexpr float FrameTime = 0.25f;

}

TEST(SpriteAnimation, StartsOnFirstFrame)
{
    const gfx::SpriteAnimation animation(Frame, 8, Fps);
    const gfx::Rect source = animation.sourceRect();

    EXPECT_EQ(animation.frame(), 0u);
    EXPECT_FLOAT_EQ(source.x, 0.0f);
    EXPECT_FLOAT_EQ(source.y, 0.0f);
    EXPECT_FLOAT_EQ(source.w, 512.0f);
    EXPECT_FLOAT_EQ(source.h, 512.0f);
}

TEST(SpriteAnimation, AdvancesOnceAFrameDurationHasElapsed)
{
    gfx::SpriteAnimation animation(Frame, 8, Fps);

    animation.update(FrameTime * 0.5f);
    EXPECT_EQ(animation.frame(), 0u);

    animation.update(FrameTime * 0.5f);
    EXPECT_EQ(animation.frame(), 1u);
}

TEST(SpriteAnimation, SourceRectFollowsFrame)
{
    gfx::SpriteAnimation animation(Frame, 8, Fps);
    animation.update(FrameTime * 3.0f);

    EXPECT_EQ(animation.frame(), 3u);
    EXPECT_FLOAT_EQ(animation.sourceRect().x, 1536.0f);
}

TEST(SpriteAnimation, WrapsAfterLastFrame)
{
    gfx::SpriteAnimation animation(Frame, 8, Fps);
    animation.update(FrameTime * 9.0f);

    EXPECT_EQ(animation.frame(), 1u);
}

TEST(SpriteAnimation, LargeDeltaSkipsFramesAndKeepsRemainder)
{
    gfx::SpriteAnimation animation(Frame, 8, Fps);

    animation.update(FrameTime * 3.5f);
    EXPECT_EQ(animation.frame(), 3u);

    animation.update(FrameTime * 0.5f);
    EXPECT_EQ(animation.frame(), 4u);
}

TEST(SpriteAnimation, StopHoldsFrameAndRestartsTiming)
{
    gfx::SpriteAnimation animation(Frame, 8, Fps);
    animation.update(FrameTime * 0.5f);

    animation.stop(2);
    EXPECT_EQ(animation.frame(), 2u);

    animation.update(FrameTime * 0.5f);     // would have completed a frame without the reset
    EXPECT_EQ(animation.frame(), 2u);

    animation.update(FrameTime * 0.5f);
    EXPECT_EQ(animation.frame(), 3u);
}

TEST(SpriteAnimation, StopWrapsOutOfRangeFrame)
{
    gfx::SpriteAnimation animation(Frame, 8, Fps);
    animation.stop(10);

    EXPECT_EQ(animation.frame(), 2u);
}

TEST(SpriteAnimation, NonPositiveFpsFreezes)
{
    for (const float fps : { 0.0f, -5.0f }) {
        gfx::SpriteAnimation animation(Frame, 8, fps);
        animation.update(10.0f);

        EXPECT_EQ(animation.frame(), 0u);
    }
}

TEST(SpriteAnimation, ZeroFrameCountBehavesAsSingleFrame)
{
    gfx::SpriteAnimation animation(Frame, 0, Fps);

    animation.update(FrameTime * 5.0f);
    EXPECT_EQ(animation.frame(), 0u);

    animation.stop(5);
    EXPECT_EQ(animation.frame(), 0u);
}
