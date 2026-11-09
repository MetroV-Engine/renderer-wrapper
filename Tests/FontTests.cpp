/*
    Created on: 11/9/26
    Filename: FontTests.cpp
    Description: Tests for Font, the glyph atlas that lays out UTF-8 text as triangles.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/Font/Font.hpp"
#include "Renderer/NullRenderer/NullRenderer.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

    constexpr float TestPixelHeight = 32.0f;

    constexpr std::string_view EAcute = "\xC3\xA9";
    constexpr std::string_view OeLigature = "\xC5\x93";
    constexpr std::string_view Euro = "\xE2\x82\xAC";
    constexpr std::string_view Emoji = "\xF0\x9F\x98\x80";

    std::vector<uint8_t> loadFontBytes()
    {
        std::ifstream file(RENDERER_TEST_FONT_PATH, std::ios::binary);
        return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>{});
    }

    void expectSameUvs(const gfx::TriangleMesh& a, const gfx::TriangleMesh& b)
    {
        ASSERT_EQ(a.vertices.size(), b.vertices.size());
        for (size_t i = 0; i < a.vertices.size(); ++i) {
            EXPECT_EQ(a.vertices[i].u, b.vertices[i].u);
            EXPECT_EQ(a.vertices[i].v, b.vertices[i].v);
        }
    }

    class FontTests : public ::testing::Test {
        protected:
            void SetUp() override
            {
                bytes = loadFontBytes();
                ASSERT_FALSE(bytes.empty()) << "could not read " << RENDERER_TEST_FONT_PATH;
                font = gfx::Font::create(renderer, bytes, TestPixelHeight);
                ASSERT_TRUE(font != nullptr);
            }

            bool rejected(const std::span<const uint8_t> ttf, const float pixelHeight)
            {
                return gfx::Font::create(renderer, ttf, pixelHeight) == nullptr;
            }

            gfx::TriangleMesh meshOf(const std::string_view text) const
            {
                gfx::TextDraw draw;
                draw.font = font.get();
                draw.text = text;

                gfx::TriangleMesh mesh;
                font->appendText(mesh, draw);
                return mesh;
            }

            void expectDifferentUvsFromQuestionMark(const std::string_view text) const
            {
                const gfx::TriangleMesh glyph = meshOf(text);
                const gfx::TriangleMesh question = meshOf("?");
                ASSERT_EQ(glyph.vertices.size(), 4u);
                ASSERT_EQ(question.vertices.size(), 4u);
                EXPECT_TRUE(glyph.vertices[0].u != question.vertices[0].u
                         || glyph.vertices[0].v != question.vertices[0].v);
            }

            gfx::NullRenderer renderer{ 800, 600 };
            std::vector<uint8_t> bytes;
            std::unique_ptr<gfx::Font> font;
    };

}

TEST_F(FontTests, CreatesAndOwnsAnAtlas)
{
    const gfx::TextureHandle texture = font->texture();
    EXPECT_NE(texture, gfx::TextureHandle::Invalid);
    EXPECT_GT(renderer.textureSize(texture).x, 0.0f);

    font.reset();

    EXPECT_EQ(renderer.textureSize(texture).x, 0.0f);
    EXPECT_EQ(renderer.textureSize(texture).y, 0.0f);
}

TEST_F(FontTests, RejectsBadInput)
{
    const std::vector<uint8_t> fourBytes(4, 0);
    const std::vector<uint8_t> zeros(64, 0);

    EXPECT_TRUE(rejected({}, TestPixelHeight));
    EXPECT_TRUE(rejected(fourBytes, TestPixelHeight));
    EXPECT_TRUE(rejected(zeros, TestPixelHeight));
    EXPECT_TRUE(rejected(bytes, 0.0f));
    EXPECT_TRUE(rejected(bytes, -1.0f));
}

TEST_F(FontTests, FailsWhenAtlasCannotFit)
{
    EXPECT_TRUE(rejected(bytes, 2000.0f));
}

TEST_F(FontTests, DrawsFourVerticesPerGlyph)
{
    const gfx::TriangleMesh mesh = meshOf("ab");

    EXPECT_EQ(mesh.vertices.size(), 8u);
    const std::vector<uint16_t> expected{ 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7 };
    EXPECT_EQ(mesh.indices, expected);
}

TEST_F(FontTests, SpacesAdvanceWithoutGeometry)
{
    EXPECT_EQ(meshOf("a b").vertices.size(), 8u);
    EXPECT_GT(font->measure("a b").x, font->measure("ab").x);
}

TEST_F(FontTests, CoversFrench)
{
    expectDifferentUvsFromQuestionMark(EAcute);
    expectDifferentUvsFromQuestionMark(OeLigature);
    expectDifferentUvsFromQuestionMark(Euro);
}

TEST_F(FontTests, MissingGlyphDrawsQuestionMark)
{
    const gfx::TriangleMesh emoji = meshOf(Emoji);

    ASSERT_EQ(emoji.vertices.size(), 4u);
    expectSameUvs(emoji, meshOf("?"));
}

TEST_F(FontTests, InvalidUtf8IsSkipped)
{
    EXPECT_EQ(meshOf("a\xFFz").vertices.size(), 8u);
    EXPECT_EQ(meshOf("a\xC3").vertices.size(), 4u);
}

TEST_F(FontTests, NewlineStartsALine)
{
    const gfx::TriangleMesh mesh = meshOf("a\na");

    ASSERT_EQ(mesh.vertices.size(), 8u);
    EXPECT_NEAR(mesh.vertices[4].y - mesh.vertices[0].y, font->lineHeight(), 1.0f);
    EXPECT_NEAR(mesh.vertices[4].x, mesh.vertices[0].x, 1.0f);
    EXPECT_FLOAT_EQ(font->measure("a\na").y, 2.0f * font->lineHeight());
}

TEST_F(FontTests, MeasureOfEmptyIsZero)
{
    const gfx::Vec2 size = font->measure("");

    EXPECT_EQ(size.x, 0.0f);
    EXPECT_EQ(size.y, 0.0f);
}

TEST_F(FontTests, TopLeftAnchorsTheFirstLine)
{
    gfx::TextDraw draw;
    draw.font = font.get();
    draw.text = "A";
    draw.position = gfx::Vec2{ 10.0f, 20.0f };

    gfx::TriangleMesh mesh;
    font->appendText(mesh, draw);

    ASSERT_EQ(mesh.vertices.size(), 4u);
    for (const gfx::Vertex& vertex : mesh.vertices) {
        EXPECT_GE(vertex.x, 10.0f);
        EXPECT_GE(vertex.y, 20.0f);
    }
}

TEST_F(FontTests, ScreenTextLandsOnWholePixels)
{
    gfx::TextDraw draw;
    draw.font = font.get();
    draw.text = "Whole pixels";
    draw.position = gfx::Vec2{ 10.0f, 20.0f };
    draw.space = gfx::Space::Screen;

    gfx::TriangleMesh mesh;
    font->appendText(mesh, draw);

    ASSERT_FALSE(mesh.vertices.empty());
    for (const gfx::Vertex& vertex : mesh.vertices) {
        EXPECT_NEAR(vertex.x, std::round(vertex.x), 1e-4);
        EXPECT_NEAR(vertex.y, std::round(vertex.y), 1e-4);
    }
}

TEST_F(FontTests, ColourReachesEveryVertex)
{
    gfx::TextDraw draw;
    draw.font = font.get();
    draw.text = "ab";
    draw.color = gfx::Color{ 10, 20, 30, 40 };

    gfx::TriangleMesh mesh;
    font->appendText(mesh, draw);

    ASSERT_EQ(mesh.vertices.size(), 8u);
    for (const gfx::Vertex& vertex : mesh.vertices) {
        EXPECT_EQ(vertex.r, 10);
        EXPECT_EQ(vertex.g, 20);
        EXPECT_EQ(vertex.b, 30);
        EXPECT_EQ(vertex.a, 40);
    }
}

TEST_F(FontTests, LongTextStopsAtTheVertexLimit)
{
    const std::string text(20000, 'a');
    const gfx::TriangleMesh mesh = meshOf(text);

    EXPECT_EQ(mesh.vertices.size(), gfx::MaxMeshVertices);
    for (const uint16_t index : mesh.indices) {
        ASSERT_LT(static_cast<size_t>(index), mesh.vertices.size());
    }
}

TEST_F(FontTests, DrawTextRecordsAMeshWithTheAtlas)
{
    renderer.beginFrame(gfx::Color{});

    gfx::TextDraw draw;
    draw.font = font.get();
    draw.text = "ab";
    draw.layer = gfx::Layer::Foreground;
    draw.space = gfx::Space::Screen;
    renderer.drawText(draw);

    ASSERT_EQ(renderer.recorded().size(), 1u);
    const gfx::NullRenderer::Recorded& entry = renderer.recorded()[0];
    EXPECT_EQ(entry.kind, gfx::NullRenderer::Recorded::Kind::Mesh);
    EXPECT_EQ(entry.meshTexture, font->texture());
    EXPECT_EQ(entry.meshLayer, gfx::Layer::Foreground);
    EXPECT_EQ(entry.meshSpace, gfx::Space::Screen);
    EXPECT_EQ(entry.meshVertices.size(), 8u);
}

TEST_F(FontTests, NullFontAndEmptyTextRecordNothing)
{
    renderer.beginFrame(gfx::Color{});

    gfx::TextDraw nullFont;
    nullFont.text = "ab";
    renderer.drawText(nullFont);

    gfx::TextDraw emptyText;
    emptyText.font = font.get();
    renderer.drawText(emptyText);

    EXPECT_TRUE(renderer.recorded().empty());
}
