/*
    Created on: 10/4/26
    Filename: TransformTests.cpp
    Description: World/screen conversion tests for Transform.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/Transform/Transform.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace {

    constexpr gfx::Vec2 Viewport{ 1280.0f, 720.0f };

    gfx::Camera2D makeCamera(const float x, const float y, const float zoom)
    {
        gfx::Camera2D camera;
        camera.center = gfx::Vec2{ x, y };
        camera.zoom = zoom;

        return camera;
    }

}

TEST(Transform, CameraCenterLandsOnViewportCenter)
{
    const gfx::Camera2D camera = makeCamera(500.0f, -300.0f, 2.5f);
    const gfx::Vec2 screen = gfx::worldToScreen(camera, camera.center, Viewport);

    EXPECT_FLOAT_EQ(screen.x, 640.0f);
    EXPECT_FLOAT_EQ(screen.y, 360.0f);
}

TEST(Transform, CameraOnHalfViewportIsIdentity)
{
    const gfx::Camera2D camera = makeCamera(640.0f, 360.0f, 1.0f);
    const gfx::Vec2 screen = gfx::worldToScreen(camera, gfx::Vec2{ 100.0f, 50.0f }, Viewport);

    EXPECT_FLOAT_EQ(screen.x, 100.0f);
    EXPECT_FLOAT_EQ(screen.y, 50.0f);
}

TEST(Transform, ZoomScalesDistanceFromCenter)
{
    const gfx::Camera2D camera = makeCamera(0.0f, 0.0f, 2.0f);
    const gfx::Vec2 screen = gfx::worldToScreen(camera, gfx::Vec2{ 10.0f, -5.0f }, Viewport);

    EXPECT_FLOAT_EQ(screen.x, 660.0f);
    EXPECT_FLOAT_EQ(screen.y, 350.0f);
}

TEST(Transform, RectMovesTopLeftAndScalesSize)
{
    const gfx::Camera2D camera = makeCamera(0.0f, 0.0f, 2.0f);
    const gfx::Rect screen = gfx::worldToScreen(camera, gfx::Rect{ 10.0f, 20.0f, 30.0f, 40.0f }, Viewport);

    EXPECT_FLOAT_EQ(screen.x, 660.0f);
    EXPECT_FLOAT_EQ(screen.y, 400.0f);
    EXPECT_FLOAT_EQ(screen.w, 60.0f);
    EXPECT_FLOAT_EQ(screen.h, 80.0f);
}

TEST(Transform, ScreenToWorldInvertsWorldToScreen)
{
    const gfx::Camera2D cameras[] = {
        makeCamera(0.0f, 0.0f, 1.0f),
        makeCamera(-750.0f, 1200.0f, 0.2f),
        makeCamera(42.0f, -17.0f, 8.0f),
    };
    const gfx::Vec2 points[] = { { 0.0f, 0.0f }, { 123.5f, -987.25f }, { -4000.0f, 600.0f } };

    for (const gfx::Camera2D& camera : cameras) {
        for (const gfx::Vec2& world : points) {
            const gfx::Vec2 back = gfx::screenToWorld(camera, gfx::worldToScreen(camera, world, Viewport), Viewport);

            EXPECT_NEAR(back.x, world.x, 1e-2f);
            EXPECT_NEAR(back.y, world.y, 1e-2f);
        }
    }
}

TEST(Transform, NonPositiveZoomActsAsOne)
{
    const gfx::Camera2D unit = makeCamera(10.0f, 20.0f, 1.0f);
    const gfx::Vec2 world{ 300.0f, -40.0f };
    const gfx::Vec2 expected = gfx::worldToScreen(unit, world, Viewport);

    for (const float zoom : { 0.0f, -2.0f }) {
        const gfx::Camera2D camera = makeCamera(10.0f, 20.0f, zoom);
        const gfx::Vec2 screen = gfx::worldToScreen(camera, world, Viewport);
        const gfx::Vec2 back = gfx::screenToWorld(camera, screen, Viewport);

        EXPECT_FLOAT_EQ(screen.x, expected.x);
        EXPECT_FLOAT_EQ(screen.y, expected.y);
        EXPECT_TRUE(std::isfinite(back.x) && std::isfinite(back.y));
        EXPECT_FLOAT_EQ(back.x, world.x);
        EXPECT_FLOAT_EQ(back.y, world.y);
    }
}
