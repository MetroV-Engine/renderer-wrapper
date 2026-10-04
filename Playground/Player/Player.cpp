/*
    Created on: 10/4/26
    Filename: Player.cpp
    Description: Playground bull: walks left/right on the ground, animated from its sheet.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Playground/Player/Player.hpp"

#include <SDL3/SDL.h>

namespace playground {

    namespace {
        constexpr float WalkSpeed = 300.0f;     // world units per second
        constexpr float FrameSize = 512.0f;
        constexpr uint32_t WalkFrames = 8;
        constexpr float WalkFps = 12.0f;
        constexpr uint32_t IdleFrame = 2;       // legs together: reads as standing
        constexpr float DrawSize = 128.0f;
        constexpr float HoofLine = 460.0f / FrameSize;  // hooves sit above the frame's bottom edge
        constexpr gfx::Layer PlayerLayer = 2;
    }

    Player::Player(const gfx::TextureHandle sheet, const gfx::Vec2 feet)
        : m_sheet(sheet)
        , m_feet(feet)
        , m_walk(gfx::Vec2{ FrameSize, FrameSize }, WalkFrames, WalkFps)
    {
        m_walk.stop(IdleFrame);
    }

    void Player::update(const float deltaSeconds)
    {
        const bool* keys = SDL_GetKeyboardState(nullptr);

        int direction = 0;
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
            --direction;
        }
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
            ++direction;
        }

        if (direction == 0) {
            m_walk.stop(IdleFrame);
            return;
        }

        // The sheet is drawn facing right.
        m_facingLeft = direction < 0;
        m_feet.x += static_cast<float>(direction) * WalkSpeed * deltaSeconds;
        m_walk.update(deltaSeconds);
    }

    void Player::stand()
    {
        m_walk.stop(IdleFrame);
    }

    gfx::SpriteDraw Player::sprite() const
    {
        gfx::SpriteDraw draw;
        draw.texture = m_sheet;
        draw.source = m_walk.sourceRect();
        draw.dest = gfx::Rect{ m_feet.x - DrawSize * 0.5f, m_feet.y - DrawSize * HoofLine, DrawSize, DrawSize };
        draw.flipX = m_facingLeft;
        draw.layer = PlayerLayer;

        return draw;
    }

    gfx::Vec2 Player::position() const
    {
        return m_feet;
    }

}
