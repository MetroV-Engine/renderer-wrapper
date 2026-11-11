/*
    Created on: 10/4/26
    Filename: ModeToggle.cpp
    Description: HUD button and Tab key switching the playground between edit and play.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Playground/ModeToggle/ModeToggle.hpp"

#include <SDL3/SDL.h>

namespace playground {

    namespace {
        constexpr gfx::Rect Button{ 4.0f, 4.0f, 28.0f, 28.0f };
        constexpr gfx::Layer ButtonLayer = gfx::Layer::Foreground;
        constexpr gfx::Layer IconLayer = gfx::Layer::Foreground;

        constexpr gfx::Color PlayColor{ 64, 170, 96, 255 };
        constexpr gfx::Color EditColor{ 72, 120, 200, 255 };

        bool contains(const gfx::Rect& rect, const float x, const float y)
        {
            return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
        }

        void drawScreenRect(gfx::IRenderer& renderer, const gfx::Rect& rect, const gfx::Color color, const gfx::Layer layer)
        {
            renderer.drawRect({ rect, color, layer, gfx::Space::Screen });
        }
    }

    ModeToggle::ModeToggle(const Mode initial)
        : m_mode(initial)
    {
    }

    bool ModeToggle::handleEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.scancode != SDL_SCANCODE_TAB || event.key.repeat) {
                return false;
            }
            toggle();
            return true;
        }

        if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
            // Mouse events are in window coordinates; screen-space draws are in pixels (differs on HiDPI).
            SDL_Window* window = SDL_GetWindowFromEvent(&event);
            const float density = window != nullptr ? SDL_GetWindowPixelDensity(window) : 0.0f;
            const float scale = density > 0.0f ? density : 1.0f;

            if (!contains(Button, event.button.x * scale, event.button.y * scale)) {
                return false;
            }
            toggle();
            return true;
        }

        return false;
    }

    Mode ModeToggle::mode() const
    {
        return m_mode;
    }

    void ModeToggle::draw(gfx::IRenderer& renderer) const
    {
        const float centerX = Button.x + Button.w * 0.5f;
        const float centerY = Button.y + Button.h * 0.5f;

        if (m_mode == Mode::Play) {
            drawScreenRect(renderer, Button, PlayColor, ButtonLayer);

            gfx::TriangleDraw icon;
            icon.points = { gfx::Vec2{ centerX - 6.0f, centerY - 8.0f }, gfx::Vec2{ centerX + 6.0f, centerY },
                            gfx::Vec2{ centerX - 6.0f, centerY + 8.0f } };
            icon.color = gfx::White;
            icon.layer = IconLayer;
            icon.space = gfx::Space::Screen;
            renderer.drawTriangle(icon);
            return;
        }

        drawScreenRect(renderer, Button, EditColor, ButtonLayer);
        drawScreenRect(renderer, gfx::Rect{ centerX - 7.0f, centerY - 8.0f, 4.0f, 16.0f }, gfx::White, IconLayer);
        drawScreenRect(renderer, gfx::Rect{ centerX + 3.0f, centerY - 8.0f, 4.0f, 16.0f }, gfx::White, IconLayer);
    }

    void ModeToggle::toggle()
    {
        m_mode = m_mode == Mode::Play ? Mode::Edit : Mode::Play;
    }

}
