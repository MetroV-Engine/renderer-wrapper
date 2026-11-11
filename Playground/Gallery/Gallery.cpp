/*
    Created on: 11/11/26
    Filename: Gallery.cpp
    Description: One labelled sample of every shape call, side by side in the level.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Playground/Gallery/Gallery.hpp"

#include "Renderer/Colors/Colors.hpp"
#include "Renderer/Font/Font.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace playground {

    namespace {
        constexpr float CellWidth = 160.0f;
        constexpr float Half = 32.0f;               // half the size of one sample
        constexpr float LabelGap = 6.0f;
        constexpr float FrameSize = 512.0f;         // one frame of the bull sheet, in texels
        constexpr float StarInner = 0.45f;          // inner radius as a fraction of the outer one
        constexpr float Pi = 3.14159265358979f;
        constexpr gfx::Layer SampleLayer = gfx::Layer::Game;

        constexpr std::array<std::string_view, 7> Labels{ "line", "triangle", "quad", "circle", "hexagon", "star", "mesh" };

        gfx::Vec2 cellCenter(const gfx::Vec2 origin, const std::size_t cell)
        {
            return gfx::Vec2{ origin.x + static_cast<float>(cell) * CellWidth, origin.y };
        }

        gfx::Vertex meshVertex(const float x, const float y, const float u, const float v, const gfx::Color color)
        {
            return gfx::Vertex{ x, y, u, v, color.r, color.g, color.b, color.a };
        }
    }

    Gallery::Gallery(const gfx::IRenderer& renderer, const gfx::TextureHandle sheet, const gfx::Font* font, const gfx::Vec2 origin)
        : m_sheet(sheet)
        , m_font(font)
        , m_origin(origin)
    {
        const gfx::Vec2 size = renderer.textureSize(sheet);
        m_frameUv = gfx::Vec2{ size.x > 0.0f ? FrameSize / size.x : 1.0f, size.y > 0.0f ? FrameSize / size.y : 1.0f };

        const gfx::Vec2 center = cellCenter(m_origin, 5);
        for (std::size_t i = 0; i < m_star.size(); ++i) {
            const float angle = -Pi * 0.5f + static_cast<float>(i) * Pi / 5.0f;
            const float radius = i % 2 == 0 ? Half : Half * StarInner;
            m_star[i] = gfx::Vec2{ center.x + radius * std::cos(angle), center.y + radius * std::sin(angle) };
        }
    }

    void Gallery::draw(gfx::IRenderer& renderer)
    {
        drawSamples(renderer);
        drawLabels(renderer);

        if (!m_misuseRequested) {
            return;
        }
        m_misuseRequested = false;
        drawMisuse(renderer);
    }

    void Gallery::requestMisuse()
    {
        m_misuseRequested = true;
    }

    void Gallery::drawSamples(gfx::IRenderer& renderer) const
    {
        const gfx::Vec2 line = cellCenter(m_origin, 0);
        gfx::LineDraw thin;
        thin.from = gfx::Vec2{ line.x - Half, line.y - Half };
        thin.to = gfx::Vec2{ line.x + Half, line.y - Half };
        thin.thickness = 1.0f;
        thin.color = gfx::White;
        thin.layer = SampleLayer;
        renderer.drawLine(thin);

        gfx::LineDraw thick = thin;
        thick.from = gfx::Vec2{ line.x - Half, line.y + Half };
        thick.to = gfx::Vec2{ line.x + Half, line.y - Half * 0.5f };
        thick.thickness = 8.0f;
        thick.color = gfx::Orange;
        renderer.drawLine(thick);

        const gfx::Vec2 tri = cellCenter(m_origin, 1);
        gfx::TriangleDraw triangle;
        triangle.points = { gfx::Vec2{ tri.x, tri.y - Half }, gfx::Vec2{ tri.x + Half, tri.y + Half }, gfx::Vec2{ tri.x - Half, tri.y + Half } };
        triangle.color = gfx::Red;
        triangle.layer = SampleLayer;
        renderer.drawTriangle(triangle);

        // Slanted on purpose: a quad that comes out as an upright rect means the corners were ignored.
        const gfx::Vec2 q = cellCenter(m_origin, 2);
        gfx::QuadDraw quad;
        quad.corners = { gfx::Vec2{ q.x - Half + 16.0f, q.y - Half }, gfx::Vec2{ q.x + Half + 8.0f, q.y - Half + 8.0f },
                         gfx::Vec2{ q.x + Half - 16.0f, q.y + Half }, gfx::Vec2{ q.x - Half - 8.0f, q.y + Half - 8.0f } };
        quad.color = gfx::Cyan;
        quad.layer = SampleLayer;
        renderer.drawQuad(quad);

        gfx::CircleDraw circle;
        circle.center = cellCenter(m_origin, 3);
        circle.radius = Half;
        circle.color = gfx::Yellow;
        circle.layer = SampleLayer;
        renderer.drawCircle(circle);

        gfx::CircleDraw hexagon = circle;
        hexagon.center = cellCenter(m_origin, 4);
        hexagon.segments = 6;
        hexagon.color = gfx::Magenta;
        renderer.drawCircle(hexagon);

        gfx::PolygonDraw star;
        star.points = m_star;
        star.color = gfx::Green;
        star.layer = SampleLayer;
        renderer.drawPolygon(star);

        // Tinted corners over the bull's first frame: the texture and the vertex colours must both show.
        const gfx::Vec2 m = cellCenter(m_origin, 6);
        const std::array<gfx::Vertex, 4> vertices{ {
            meshVertex(m.x - Half, m.y - Half, 0.0f, 0.0f, gfx::White),
            meshVertex(m.x + Half, m.y - Half, m_frameUv.x, 0.0f, gfx::Yellow),
            meshVertex(m.x + Half, m.y + Half, m_frameUv.x, m_frameUv.y, gfx::Cyan),
            meshVertex(m.x - Half, m.y + Half, 0.0f, m_frameUv.y, gfx::Magenta) } };
        const std::array<uint16_t, 6> indices{ 0, 1, 2, 0, 2, 3 };
        renderer.drawMesh(gfx::MeshDraw{ vertices, indices, m_sheet, SampleLayer, gfx::Space::World });
    }

    void Gallery::drawLabels(gfx::IRenderer& renderer) const
    {
        if (m_font == nullptr) {
            return;
        }

        for (std::size_t i = 0; i < Labels.size(); ++i) {
            const gfx::Vec2 center = cellCenter(m_origin, i);
            const gfx::Vec2 size = m_font->measure(Labels[i]);

            gfx::TextDraw label;
            label.font = m_font;
            label.text = Labels[i];
            label.position = gfx::Vec2{ center.x - size.x * 0.5f, center.y + Half + LabelGap };
            label.layer = SampleLayer;
            renderer.drawText(label);
        }
    }

    void Gallery::drawMisuse(gfx::IRenderer& renderer) const
    {
        std::cerr << "[Gallery] forcing misuse: expect one warning each for a flat polygon, a malformed mesh and a null font"
                     " (first press only); the 2-point polygon draws nothing and says nothing\n";

        const std::array<gfx::Vec2, 2> twoPoints{ m_origin, gfx::Vec2{ m_origin.x + Half, m_origin.y } };
        renderer.drawPolygon(gfx::PolygonDraw{ twoPoints, gfx::White, SampleLayer, gfx::Space::World });

        // All three on one line: zero area, so the triangulator must refuse it.
        const std::array<gfx::Vec2, 3> flat{ m_origin, gfx::Vec2{ m_origin.x + Half, m_origin.y },
                                             gfx::Vec2{ m_origin.x + Half * 2.0f, m_origin.y } };
        renderer.drawPolygon(gfx::PolygonDraw{ flat, gfx::White, SampleLayer, gfx::Space::World });

        const std::array<gfx::Vertex, 3> vertices{ {
            meshVertex(m_origin.x, m_origin.y, 0.0f, 0.0f, gfx::White),
            meshVertex(m_origin.x + Half, m_origin.y, 0.0f, 0.0f, gfx::White),
            meshVertex(m_origin.x, m_origin.y + Half, 0.0f, 0.0f, gfx::White) } };
        const std::array<uint16_t, 3> outOfRange{ 0, 1, 7 };
        renderer.drawMesh(gfx::MeshDraw{ vertices, outOfRange, gfx::TextureHandle::Invalid, SampleLayer, gfx::Space::World });

        gfx::TextDraw noFont;
        noFont.text = "no font";
        noFont.position = m_origin;
        noFont.layer = SampleLayer;
        renderer.drawText(noFont);
    }

}
