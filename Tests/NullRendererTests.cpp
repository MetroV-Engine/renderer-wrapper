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

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace {

    using Kind = gfx::NullRenderer::Recorded::Kind;

    std::vector<uint8_t> pixels(const uint32_t width, const uint32_t height)
    {
        return std::vector<uint8_t>(static_cast<size_t>(width) * height * 4, 255);
    }

    gfx::RectDraw someRect()
    {
        return gfx::RectDraw{ gfx::Rect{ 1.0f, 2.0f, 3.0f, 4.0f }, gfx::Color{ 10, 20, 30, 40 }, gfx::Layer::Foreground, gfx::Space::Screen };
    }

    std::vector<gfx::Vertex> threeVertices()
    {
        std::vector<gfx::Vertex> vertices(3);
        vertices[1].x = 10.0f;
        vertices[2].y = 10.0f;
        return vertices;
    }

    gfx::MeshDraw meshOf(const std::vector<gfx::Vertex>& vertices, const std::vector<uint16_t>& indices)
    {
        gfx::MeshDraw draw;
        draw.vertices = vertices;
        draw.indices = indices;
        return draw;
    }

}

TEST(Layer, DrawsBackgroundThenGameThenForeground)
{
    EXPECT_EQ(gfx::layerSlot(gfx::Layer::Background), 0);
    EXPECT_EQ(gfx::layerSlot(gfx::Layer::Game), 1);
    EXPECT_EQ(gfx::layerSlot(gfx::Layer::Foreground), 2);
}

TEST(Layer, InvalidValueFallsBackToGame)
{
    EXPECT_EQ(gfx::layerSlot(static_cast<gfx::Layer>(99)), 1);
}

TEST(Layer, DrawsDefaultToGame)
{
    EXPECT_EQ(gfx::RectDraw{}.layer, gfx::Layer::Game);
    EXPECT_EQ(gfx::SpriteDraw{}.layer, gfx::Layer::Game);
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
    sprite.layer = gfx::Layer::Background;
    sprite.tint = gfx::Color{ 1, 2, 3, 4 };

    renderer.beginFrame(gfx::Black);
    renderer.drawRect(someRect());
    renderer.drawSprite(sprite);
    renderer.endFrame();

    ASSERT_EQ(renderer.recorded().size(), 2u);

    const gfx::RectDraw& rect = renderer.recorded()[0].rect;
    EXPECT_FLOAT_EQ(rect.dest.w, 3.0f);
    EXPECT_EQ(rect.color.a, 40);
    EXPECT_EQ(rect.layer, gfx::Layer::Foreground);
    EXPECT_EQ(rect.space, gfx::Space::Screen);

    const gfx::SpriteDraw& recorded = renderer.recorded()[1].sprite;
    EXPECT_EQ(recorded.texture, sprite.texture);
    EXPECT_FLOAT_EQ(recorded.source.x, 512.0f);
    EXPECT_FLOAT_EQ(recorded.rotation, 1.5f);
    EXPECT_TRUE(recorded.flipX);
    EXPECT_EQ(recorded.layer, gfx::Layer::Background);
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

TEST(NullRenderer, EveryShapeIsRecordedWithItsKind)
{
    gfx::NullRenderer renderer(800, 600);
    const std::array<gfx::Vec2, 3> points{ gfx::Vec2{ 0.0f, 0.0f }, gfx::Vec2{ 4.0f, 0.0f }, gfx::Vec2{ 0.0f, 4.0f } };
    const std::vector<gfx::Vertex> vertices = threeVertices();
    const std::vector<uint16_t> indices{ 0, 1, 2 };

    gfx::LineDraw line;
    line.thickness = 2.5f;
    gfx::TriangleDraw triangle;
    triangle.points = points;
    gfx::QuadDraw quad;
    quad.corners[2] = gfx::Vec2{ 7.0f, 8.0f };
    gfx::CircleDraw circle;
    circle.radius = 9.0f;
    circle.segments = 12;
    gfx::PolygonDraw polygon;
    polygon.points = points;
    gfx::TextDraw text;
    text.text = "hi";
    text.position = gfx::Vec2{ 3.0f, 6.0f };

    renderer.beginFrame(gfx::Black);
    renderer.drawLine(line);
    renderer.drawTriangle(triangle);
    renderer.drawQuad(quad);
    renderer.drawCircle(circle);
    renderer.drawPolygon(polygon);
    renderer.drawMesh(meshOf(vertices, indices));
    renderer.drawText(text);
    renderer.endFrame();

    const std::vector<gfx::NullRenderer::Recorded>& recorded = renderer.recorded();
    ASSERT_EQ(recorded.size(), 7u);
    EXPECT_EQ(recorded[0].kind, Kind::Line);
    EXPECT_EQ(recorded[1].kind, Kind::Triangle);
    EXPECT_EQ(recorded[2].kind, Kind::Quad);
    EXPECT_EQ(recorded[3].kind, Kind::Circle);
    EXPECT_EQ(recorded[4].kind, Kind::Polygon);
    EXPECT_EQ(recorded[5].kind, Kind::Mesh);
    EXPECT_EQ(recorded[6].kind, Kind::Text);

    EXPECT_FLOAT_EQ(recorded[0].line.thickness, 2.5f);
    EXPECT_FLOAT_EQ(recorded[1].triangle.points[1].x, 4.0f);
    EXPECT_FLOAT_EQ(recorded[2].quad.corners[2].y, 8.0f);
    EXPECT_FLOAT_EQ(recorded[3].circle.radius, 9.0f);
    EXPECT_EQ(recorded[3].circle.segments, 12);
    EXPECT_EQ(recorded[4].polygonPoints.size(), 3u);
    EXPECT_EQ(recorded[5].meshVertices.size(), 3u);
    EXPECT_EQ(recorded[6].textContent, "hi");
    EXPECT_FLOAT_EQ(recorded[6].text.position.y, 6.0f);
}

TEST(NullRenderer, PolygonKeepsItsOwnPoints)
{
    gfx::NullRenderer renderer(800, 600);
    std::vector<gfx::Vec2> points{ gfx::Vec2{ 1.0f, 2.0f }, gfx::Vec2{ 5.0f, 2.0f }, gfx::Vec2{ 5.0f, 6.0f } };

    gfx::PolygonDraw polygon;
    polygon.points = points;

    renderer.beginFrame(gfx::Black);
    renderer.drawPolygon(polygon);
    renderer.endFrame();

    points[0].x = 99.0f;
    points.clear();

    ASSERT_EQ(renderer.recorded().size(), 1u);
    const std::vector<gfx::Vec2>& kept = renderer.recorded()[0].polygonPoints;
    ASSERT_EQ(kept.size(), 3u);
    EXPECT_FLOAT_EQ(kept[0].x, 1.0f);
    EXPECT_FLOAT_EQ(kept[2].y, 6.0f);
}

TEST(NullRenderer, TextKeepsItsOwnString)
{
    gfx::NullRenderer renderer(800, 600);
    const gfx::Font* const font = reinterpret_cast<const gfx::Font*>(std::uintptr_t{ 0x1000 });

    renderer.beginFrame(gfx::Black);
    {
        const std::string scoped = "scoped text";
        gfx::TextDraw draw;
        draw.font = font;
        draw.text = scoped;
        renderer.drawText(draw);
    }
    renderer.endFrame();

    ASSERT_EQ(renderer.recorded().size(), 1u);
    EXPECT_EQ(renderer.recorded()[0].textContent, "scoped text");
    EXPECT_EQ(renderer.recorded()[0].text.font, font);
}

TEST(NullRenderer, ShapesOutsideFrameAreIgnored)
{
    gfx::NullRenderer renderer(800, 600);
    const std::array<gfx::Vec2, 3> points{ gfx::Vec2{ 0.0f, 0.0f }, gfx::Vec2{ 4.0f, 0.0f }, gfx::Vec2{ 0.0f, 4.0f } };

    gfx::PolygonDraw polygon;
    polygon.points = points;
    gfx::TextDraw text;
    text.text = "hi";

    renderer.drawLine(gfx::LineDraw{});
    renderer.drawTriangle(gfx::TriangleDraw{});
    renderer.drawQuad(gfx::QuadDraw{});
    renderer.drawCircle(gfx::CircleDraw{});
    renderer.drawPolygon(polygon);
    renderer.drawText(text);
    renderer.beginFrame(gfx::Black);
    renderer.endFrame();
    renderer.drawLine(gfx::LineDraw{});
    renderer.drawText(text);

    EXPECT_TRUE(renderer.recorded().empty());
}

TEST(NullRenderer, MeshKeepsItsOwnCopy)
{
    gfx::NullRenderer renderer(800, 600);
    std::vector<gfx::Vertex> vertices = threeVertices();
    std::vector<uint16_t> indices{ 0, 1, 2 };

    renderer.beginFrame(gfx::Black);
    renderer.drawMesh(meshOf(vertices, indices));
    renderer.endFrame();

    vertices[1].x = 99.0f;
    indices[2] = 0;
    vertices.clear();
    indices.clear();

    ASSERT_EQ(renderer.recorded().size(), 1u);

    const gfx::NullRenderer::Recorded& entry = renderer.recorded()[0];
    ASSERT_EQ(entry.meshVertices.size(), 3u);
    ASSERT_EQ(entry.meshIndices.size(), 3u);
    EXPECT_FLOAT_EQ(entry.meshVertices[1].x, 10.0f);
    EXPECT_EQ(entry.meshIndices[2], 2);
}

TEST(NullRenderer, RejectsMalformedMesh)
{
    gfx::NullRenderer renderer(800, 600);
    const std::vector<gfx::Vertex> vertices = threeVertices();
    const std::vector<uint16_t> twoIndices{ 0, 1 };
    const std::vector<uint16_t> pastTheEnd{ 0, 1, 3 };

    renderer.beginFrame(gfx::Black);
    renderer.drawMesh(meshOf(vertices, twoIndices));
    renderer.drawMesh(meshOf(vertices, pastTheEnd));
    renderer.endFrame();

    EXPECT_TRUE(renderer.recorded().empty());
}

TEST(NullRenderer, RejectsUnknownMeshTexture)
{
    gfx::NullRenderer renderer(800, 600);
    const std::vector<gfx::Vertex> vertices = threeVertices();
    const std::vector<uint16_t> indices{ 0, 1, 2 };

    gfx::MeshDraw draw = meshOf(vertices, indices);
    draw.texture = static_cast<gfx::TextureHandle>(999);

    renderer.beginFrame(gfx::Black);
    renderer.drawMesh(draw);
    EXPECT_TRUE(renderer.recorded().empty());

    draw.texture = gfx::TextureHandle::Invalid;
    renderer.drawMesh(draw);
    renderer.endFrame();

    ASSERT_EQ(renderer.recorded().size(), 1u);
    EXPECT_EQ(renderer.recorded()[0].meshTexture, gfx::TextureHandle::Invalid);
}

TEST(NullRenderer, EmptyMeshRecordsNothing)
{
    gfx::NullRenderer renderer(800, 600);
    const std::vector<gfx::Vertex> vertices = threeVertices();
    const std::vector<uint16_t> indices{ 0, 1, 2 };

    renderer.beginFrame(gfx::Black);
    renderer.drawMesh(gfx::MeshDraw{});
    renderer.drawMesh(meshOf(vertices, {}));
    renderer.drawMesh(meshOf({}, indices));
    renderer.endFrame();

    EXPECT_TRUE(renderer.recorded().empty());
}

TEST(NullRenderer, MeshOutsideFrameIsIgnored)
{
    gfx::NullRenderer renderer(800, 600);
    const std::vector<gfx::Vertex> vertices = threeVertices();
    const std::vector<uint16_t> indices{ 0, 1, 2 };

    renderer.drawMesh(meshOf(vertices, indices));
    renderer.beginFrame(gfx::Black);
    renderer.endFrame();
    renderer.drawMesh(meshOf(vertices, indices));

    EXPECT_TRUE(renderer.recorded().empty());
}
