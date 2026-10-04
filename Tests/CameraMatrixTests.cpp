/*
    Created on: 10/4/26
    Filename: CameraMatrixTests.cpp
    Description: GPU camera matrix tests: must match worldToScreen.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/CameraMatrix/CameraMatrix.hpp"
#include "Renderer/Transform/Transform.hpp"

#include <gtest/gtest.h>

namespace {

    constexpr gfx::Vec2 Viewport{ 1280.0f, 720.0f };

    gfx::Camera2D makeCamera(const float x, const float y, const float zoom)
    {
        gfx::Camera2D camera;
        camera.center = gfx::Vec2{ x, y };
        camera.zoom = zoom;

        return camera;
    }

    // Column-major: x' = m0*x + m4*y + m12, y' = m1*x + m5*y + m13.
    gfx::Vec2 apply(const gfx::Mat4& m, const gfx::Vec2 p)
    {
        return gfx::Vec2{ m[0] * p.x + m[4] * p.y + m[12], m[1] * p.x + m[5] * p.y + m[13] };
    }

}

TEST(CameraMatrix, MatchesWorldToScreen)
{
    const gfx::Camera2D cameras[] = {
        makeCamera(640.0f, 360.0f, 1.0f),
        makeCamera(0.0f, 0.0f, 2.0f),
        makeCamera(-750.0f, 1200.0f, 0.2f),
        makeCamera(42.0f, -17.0f, 8.0f),
        makeCamera(10.0f, 20.0f, 0.0f),
    };
    const gfx::Vec2 points[] = { { 0.0f, 0.0f }, { 123.5f, -987.25f }, { -4000.0f, 600.0f }, { 3000.0f, 3000.0f } };

    for (const gfx::Camera2D& camera : cameras) {
        const gfx::Mat4 matrix = gfx::cameraMatrix(camera, Viewport);

        for (const gfx::Vec2& world : points) {
            const gfx::Vec2 expected = gfx::worldToScreen(camera, world, Viewport);
            const gfx::Vec2 actual = apply(matrix, world);

            EXPECT_NEAR(actual.x, expected.x, 1e-2f);
            EXPECT_NEAR(actual.y, expected.y, 1e-2f);
        }
    }
}

TEST(CameraMatrix, NonPositiveZoomActsAsOne)
{
    const gfx::Mat4 unit = gfx::cameraMatrix(makeCamera(10.0f, 20.0f, 1.0f), Viewport);

    for (const float zoom : { 0.0f, -3.0f }) {
        EXPECT_EQ(gfx::cameraMatrix(makeCamera(10.0f, 20.0f, zoom), Viewport), unit);
    }
}

TEST(CameraMatrix, DefaultCameraIsIdentity)
{
    EXPECT_EQ(gfx::cameraMatrix(makeCamera(640.0f, 360.0f, 1.0f), Viewport), gfx::identityMatrix());
}

TEST(CameraMatrix, IsColumnMajor2DAffine)
{
    const gfx::Mat4 m = gfx::cameraMatrix(makeCamera(100.0f, 50.0f, 2.0f), Viewport);

    EXPECT_FLOAT_EQ(m[0], 2.0f);
    EXPECT_FLOAT_EQ(m[5], 2.0f);
    EXPECT_FLOAT_EQ(m[10], 1.0f);
    EXPECT_FLOAT_EQ(m[15], 1.0f);
    EXPECT_FLOAT_EQ(m[12], 640.0f - 200.0f);
    EXPECT_FLOAT_EQ(m[13], 360.0f - 100.0f);
    EXPECT_FLOAT_EQ(m[3], 0.0f);
    EXPECT_FLOAT_EQ(m[7], 0.0f);
}

TEST(CameraMatrix, IdentityIsIdentity)
{
    const gfx::Mat4 m = gfx::identityMatrix();

    for (size_t i = 0; i < 16; ++i) {
        EXPECT_FLOAT_EQ(m[i], (i % 5 == 0) ? 1.0f : 0.0f);
    }
}
