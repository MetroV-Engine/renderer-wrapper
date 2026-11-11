/*
    Created on: 10/4/26
    Filename: Showcase.cpp
    Description: Exercises the IRenderer paths the bull does not reach.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Playground/Showcase/Showcase.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>

namespace playground {

    namespace {
        constexpr float BullSize = 128.0f;
        constexpr float FrameSize = 512.0f;
        constexpr float StandingFrame = 2.0f;
        constexpr float SpinSpeed = 1.0f;       // radians per second
        constexpr float HudStripHeight = 28.0f;
        constexpr float HudMargin = 4.0f;
        constexpr gfx::Layer BullLayer = gfx::Layer::Game;
        constexpr gfx::Layer HudLayer = gfx::Layer::Foreground;

        uint8_t wave(const float time, const float phase)
        {
            return static_cast<uint8_t>(128.0f + 127.0f * std::sin(time + phase));
        }
    }

    Showcase::Showcase(gfx::IRenderer& renderer, const tools::Image& sheet, const gfx::Vec2 center)
        : m_renderer(renderer)
        , m_sheet(sheet)
        , m_center(center)
    {
        m_texture = m_renderer.createTexture(m_sheet.rgba.data(), m_sheet.width, m_sheet.height);
        m_sheetSize = m_renderer.textureSize(m_texture);

        std::cerr << "[Showcase] textureSize: " << m_sheetSize.x << "x" << m_sheetSize.y << "\n";
    }

    Showcase::~Showcase()
    {
        m_renderer.destroyTexture(m_texture);
    }

    void Showcase::update(const float deltaSeconds)
    {
        m_time += deltaSeconds;
    }

    void Showcase::draw(const gfx::Vec2 viewport)
    {
        gfx::SpriteDraw bull;
        bull.texture = m_texture;
        bull.source = gfx::Rect{ StandingFrame * FrameSize, 0.0f, FrameSize, FrameSize };
        bull.dest = gfx::Rect{ m_center.x - BullSize * 0.5f, m_center.y - BullSize * 0.5f, BullSize, BullSize };
        bull.rotation = m_time * SpinSpeed;
        bull.tint = gfx::Color{ wave(m_time, 0.0f), wave(m_time, 2.1f), wave(m_time, 4.2f), 255 };
        bull.layer = BullLayer;
        m_renderer.drawSprite(bull);

        if (m_sheetSize.y > 0.0f) {
            const float width = HudStripHeight * m_sheetSize.x / m_sheetSize.y;

            gfx::SpriteDraw strip;
            strip.texture = m_texture;      // source left empty: the whole sheet
            strip.dest = gfx::Rect{ viewport.x - width - HudMargin, HudMargin, width, HudStripHeight };
            strip.layer = HudLayer;
            strip.space = gfx::Space::Screen;
            m_renderer.drawSprite(strip);
        }

        if (!m_reloadRequested) {
            return;
        }
        m_reloadRequested = false;

        // The draws above still reference the old handle: the backend must defer its release.
        const gfx::TextureHandle previous = m_texture;
        m_renderer.destroyTexture(previous);
        m_texture = m_renderer.createTexture(m_sheet.rgba.data(), m_sheet.width, m_sheet.height);

        std::cerr << "[Showcase] reloaded mid-frame: handle " << static_cast<uint32_t>(previous)
                  << " -> " << static_cast<uint32_t>(m_texture) << "\n";
    }

    void Showcase::requestReload()
    {
        m_reloadRequested = true;
    }

    void Showcase::misuse()
    {
        std::cerr << "[Showcase] forcing misuse: expect one warning per call (first press only for one-shot ones)\n";

        const gfx::RectDraw marker{ gfx::Rect{ 0.0f, 0.0f, 8.0f, 8.0f }, gfx::White, gfx::Layer::Foreground, gfx::Space::Screen };

        m_renderer.drawRect(marker);                // outside a frame
        m_renderer.beginFrame(gfx::Black);
        m_renderer.beginFrame(gfx::Black);          // twice

        m_renderer.drawSprite(gfx::SpriteDraw{});   // invalid texture handle

        m_renderer.endFrame();
        m_renderer.endFrame();                      // without beginFrame
    }

}
