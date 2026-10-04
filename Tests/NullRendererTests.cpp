/*
    Created on: 10/4/26
    Filename: NullRendererTests.cpp
    Description: Contract tests for NullRenderer, the headless IRenderer.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/NullRenderer/NullRenderer.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace {

    using Kind = gfx::NullRenderer::Recorded::Kind;

    std::vector<uint8_t> pixels(const uint32_t width, const uint32_t height)
    {
        return std::vector<uint8_t>(static_cast<size_t>(width) * height * 4, 255);
    }

    gfx::RectDraw someRect()
    {
        return gfx::RectDraw{ gfx::Rect{ 1.0f, 2.0f, 3.0f, 4.0f }, gfx::Color{ 10, 20, 30, 40 }, 5, gfx::Space::Screen };
    }

}

TEST(NullRenderer, CameraStartsCenteredOnViewport)
{
    const gfx::NullRenderer renderer(800, 600);

    EXPECT_FLOAT_EQ(renderer.camera().center.x, 400.0f);
    EXPECT_FLOAT_EQ(renderer.camera().center.y, 300.0f);
    EXPECT_FLOAT_EQ(renderer.camera().zoom, 1.0f);
}

TEST(NullRenderer, RecordsDrawsOnlyInsideFrame)
{
    gfx::NullRenderer renderer(800, 600);

    renderer.drawRect(someRect());
    renderer.beginFrame(gfx::Black);
    renderer.drawRect(someRect());
    renderer.drawSprite(gfx::SpriteDraw{});
    renderer.endFrame();
    renderer.drawSprite(gfx::SpriteDraw{});

    ASSERT_EQ(renderer.recorded().size(), 2u);
    EXPECT_EQ(renderer.recorded()[0].kind, Kind::Rect);
    EXPECT_EQ(renderer.recorded()[1].kind, Kind::Sprite);
}

TEST(NullRenderer, KeepsDrawPayloads)
{
    gfx::NullRenderer renderer(800, 600);

    gfx::SpriteDraw sprite;
    sprite.texture = static_cast<gfx::TextureHandle>(7);
    sprite.source = gfx::Rect{ 512.0f, 0.0f, 512.0f, 512.0f };
    sprite.rotation = 1.5f;
    sprite.flipX = true;
    sprite.layer = 9;
    sprite.tint = gfx::Color{ 1, 2, 3, 4 };

    renderer.beginFrame(gfx::Black);
    renderer.drawRect(someRect());
    renderer.drawSprite(sprite);
    renderer.endFrame();

    ASSERT_EQ(renderer.recorded().size(), 2u);

    const gfx::RectDraw& rect = renderer.recorded()[0].rect;
    EXPECT_FLOAT_EQ(rect.dest.w, 3.0f);
    EXPECT_EQ(rect.color.a, 40);
    EXPECT_EQ(rect.layer, 5);
    EXPECT_EQ(rect.space, gfx::Space::Screen);

    const gfx::SpriteDraw& recorded = renderer.recorded()[1].sprite;
    EXPECT_EQ(recorded.texture, sprite.texture);
    EXPECT_FLOAT_EQ(recorded.source.x, 512.0f);
    EXPECT_FLOAT_EQ(recorded.rotation, 1.5f);
    EXPECT_TRUE(recorded.flipX);
    EXPECT_EQ(recorded.layer, 9);
    EXPECT_EQ(recorded.tint.b, 3);
}

TEST(NullRenderer, CountsFramesAndRemembersClearColor)
{
    gfx::NullRenderer renderer(800, 600);

    renderer.beginFrame(gfx::Color{ 24, 26, 32, 255 });
    renderer.endFrame();
    renderer.beginFrame(gfx::White);
    renderer.endFrame();

    EXPECT_EQ(renderer.framesPresented(), 2u);
    EXPECT_EQ(renderer.lastClear().r, 255);
}

TEST(NullRenderer, StoresCamera)
{
    gfx::NullRenderer renderer(800, 600);

    gfx::Camera2D camera;
    camera.center = gfx::Vec2{ -12.0f, 34.0f };
    camera.zoom = 3.0f;
    renderer.setCamera(camera);

    EXPECT_FLOAT_EQ(renderer.camera().center.x, -12.0f);
    EXPECT_FLOAT_EQ(renderer.camera().center.y, 34.0f);
    EXPECT_FLOAT_EQ(renderer.camera().zoom, 3.0f);
}

TEST(NullRenderer, CreatesDistinctTexturesWithTheirSize)
{
    gfx::NullRenderer renderer(800, 600);
    const std::vector<uint8_t> wide = pixels(4, 1);
    const std::vector<uint8_t> tall = pixels(2, 3);

    const gfx::TextureHandle first = renderer.createTexture(wide.data(), 4, 1);
    const gfx::TextureHandle second = renderer.createTexture(tall.data(), 2, 3);

    EXPECT_NE(first, gfx::TextureHandle::Invalid);
    EXPECT_NE(second, gfx::TextureHandle::Invalid);
    EXPECT_NE(first, second);
    EXPECT_FLOAT_EQ(renderer.textureSize(first).x, 4.0f);
    EXPECT_FLOAT_EQ(renderer.textureSize(first).y, 1.0f);
    EXPECT_FLOAT_EQ(renderer.textureSize(second).x, 2.0f);
    EXPECT_FLOAT_EQ(renderer.textureSize(second).y, 3.0f);
}

TEST(NullRenderer, RejectsEmptyTextureInput)
{
    gfx::NullRenderer renderer(800, 600);
    const std::vector<uint8_t> data = pixels(2, 2);

    EXPECT_EQ(renderer.createTexture(nullptr, 2, 2), gfx::TextureHandle::Invalid);
    EXPECT_EQ(renderer.createTexture(data.data(), 0, 2), gfx::TextureHandle::Invalid);
    EXPECT_EQ(renderer.createTexture(data.data(), 2, 0), gfx::TextureHandle::Invalid);
}

TEST(NullRenderer, DestroyedOrUnknownTextureHasZeroSize)
{
    gfx::NullRenderer renderer(800, 600);
    const std::vector<uint8_t> data = pixels(2, 2);
    const gfx::TextureHandle texture = renderer.createTexture(data.data(), 2, 2);

    renderer.destroyTexture(texture);
    renderer.destroyTexture(texture);                       // twice: must be harmless
    renderer.destroyTexture(gfx::TextureHandle::Invalid);

    EXPECT_FLOAT_EQ(renderer.textureSize(texture).x, 0.0f);
    EXPECT_FLOAT_EQ(renderer.textureSize(gfx::TextureHandle::Invalid).y, 0.0f);
}

TEST(NullRenderer, ClearRecordedEmptiesTheLog)
{
    gfx::NullRenderer renderer(800, 600);

    renderer.beginFrame(gfx::Black);
    renderer.drawRect(someRect());
    renderer.endFrame();
    renderer.clearRecorded();

    EXPECT_TRUE(renderer.recorded().empty());
}
