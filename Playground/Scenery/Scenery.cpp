/*
    Created on: 11/9/26
    Filename: Scenery.cpp
    Description: The level's backdrop (sky, sun, hills) and the bushes the player walks behind.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Playground/Scenery/Scenery.hpp"

#include "Renderer/Colors/Colors.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace playground {

    namespace {
        constexpr float LevelLeft = -2000.0f;
        constexpr int HillSegments = 40;
        constexpr float HillStep = 150.0f;
        constexpr float HillWavelength = 0.0021f;   // radians per world unit
        constexpr float HillBelowGround = 20.0f;    // the ground rect hides the hills' flat base

        constexpr gfx::Color SkyTop = gfx::fromHex(0x1B2A49FF);
        constexpr gfx::Color SkyHorizon = gfx::fromHex(0xE08A5CFF);
        constexpr gfx::Color FarHillColor = gfx::fromHex(0x4A3F5CFF);
        constexpr gfx::Color NearHillColor = gfx::fromHex(0x2E3345FF);
        constexpr gfx::Color BushColor = gfx::fromHex(0x1F3327FF);

        constexpr float SunRadius = 56.0f;
        constexpr float GlowRadius = 96.0f;
        constexpr uint8_t GlowAlpha = 70;

        constexpr std::array<float, 4> BushPositions{ -420.0f, 300.0f, 860.0f, 1500.0f };
        constexpr float BushSink = 6.0f;            // circle centres sit just below the ground line

        struct BushLobe {
            float offset;
            float radius;
        };
        constexpr std::array<BushLobe, 3> BushLobes{ { { -44.0f, 38.0f }, { 0.0f, 52.0f }, { 44.0f, 38.0f } } };

        std::vector<gfx::Vec2> hillOutline(const float groundTop, const float minHeight, const float swell, const float phase)
        {
            std::vector<gfx::Vec2> outline;
            outline.reserve(HillSegments + 3);

            for (int i = 0; i <= HillSegments; ++i) {
                const float x = LevelLeft + static_cast<float>(i) * HillStep;
                const float rise = 0.5f + 0.5f * std::sin(x * HillWavelength + phase);
                outline.push_back(gfx::Vec2{ x, groundTop - minHeight - swell * rise });
            }

            const float right = LevelLeft + static_cast<float>(HillSegments) * HillStep;
            outline.push_back(gfx::Vec2{ right, groundTop + HillBelowGround });
            outline.push_back(gfx::Vec2{ LevelLeft, groundTop + HillBelowGround });

            return outline;
        }

        gfx::Vertex skyVertex(const float x, const float y, const gfx::Color color)
        {
            return gfx::Vertex{ x, y, 0.0f, 0.0f, color.r, color.g, color.b, color.a };
        }
    }

    Scenery::Scenery(const float groundTop)
        : m_groundTop(groundTop)
        , m_farHills(hillOutline(groundTop, 150.0f, 170.0f, 0.0f))
        , m_nearHills(hillOutline(groundTop, 50.0f, 90.0f, 2.4f))
    {
    }

    void Scenery::drawBackground(gfx::IRenderer& renderer, const gfx::Vec2 viewport) const
    {
        const std::array<gfx::Vertex, 4> sky{ {
            skyVertex(0.0f, 0.0f, SkyTop),
            skyVertex(viewport.x, 0.0f, SkyTop),
            skyVertex(viewport.x, viewport.y, SkyHorizon),
            skyVertex(0.0f, viewport.y, SkyHorizon) } };
        const std::array<uint16_t, 6> skyIndices{ 0, 1, 2, 0, 2, 3 };
        renderer.drawMesh(gfx::MeshDraw{ sky, skyIndices, gfx::TextureHandle::Invalid, gfx::Layer::Background, gfx::Space::Screen });

        // Screen space: something this far away does not move with the camera.
        const gfx::Vec2 sunCenter{ viewport.x * 0.72f, viewport.y * 0.32f };
        const gfx::Color sun = gfx::fromHsv(gfx::Hsv{ 40.0f, 0.65f, 1.0f });
        renderer.drawCircle({ sunCenter, GlowRadius, gfx::withAlpha(sun, GlowAlpha), 0, gfx::Layer::Background, gfx::Space::Screen });
        renderer.drawCircle({ sunCenter, SunRadius, sun, 0, gfx::Layer::Background, gfx::Space::Screen });

        renderer.drawPolygon({ m_farHills, FarHillColor, gfx::Layer::Background, gfx::Space::World });
        renderer.drawPolygon({ m_nearHills, NearHillColor, gfx::Layer::Background, gfx::Space::World });
    }

    void Scenery::drawForeground(gfx::IRenderer& renderer) const
    {
        for (const float bushX : BushPositions) {
            for (const BushLobe& lobe : BushLobes) {
                const gfx::Vec2 center{ bushX + lobe.offset, m_groundTop + BushSink };
                renderer.drawCircle({ center, lobe.radius, BushColor, 0, gfx::Layer::Foreground, gfx::Space::World });
            }
        }
    }

}
