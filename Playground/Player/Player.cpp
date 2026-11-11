/*
    Created on: 10/4/26
    Filename: Player.cpp
    Description: Playground bull: idles, walks left/right and hops on the ground, animated from its sheets.

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
        constexpr uint32_t IdleFrames = 12;     // one breath, then a tail swish
        constexpr float IdleFps = 6.0f;
        constexpr float DrawSize = 128.0f;
        constexpr float HoofLine = 460.0f / FrameSize;  // hooves sit above the frame's bottom edge
        constexpr gfx::Layer PlayerLayer = gfx::Layer::Game;

        // About one bull high (v^2 / 2g = 120) and 0.6 s in the air.
        constexpr float JumpSpeed = 800.0f;
        constexpr float Gravity = 2667.0f;
        constexpr float LandingSeconds = 0.1f;

        constexpr float RisingFrame = 0.0f;
        constexpr float FallingFrame = 1.0f;
        constexpr float LandingFrame = 2.0f;
    }

    Player::Player(const gfx::TextureHandle walkSheet, const gfx::TextureHandle idleSheet, const gfx::TextureHandle jumpSheet,
                   const gfx::Vec2 feet)
        : m_walkSheet(walkSheet)
        , m_idleSheet(idleSheet)
        , m_jumpSheet(jumpSheet)
        , m_feet(feet)
        , m_groundY(feet.y)
        , m_walk(gfx::Vec2{ FrameSize, FrameSize }, WalkFrames, WalkFps)
        , m_idle(gfx::Vec2{ FrameSize, FrameSize }, IdleFrames, IdleFps)
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

        if (direction != 0) {
            // The sheets are drawn facing right.
            m_facingLeft = direction < 0;
            m_feet.x += static_cast<float>(direction) * WalkSpeed * deltaSeconds;
        }

        if (m_airborne) {
            m_velocityY += Gravity * deltaSeconds;
            m_feet.y += m_velocityY * deltaSeconds;
            if (m_feet.y >= m_groundY) {
                m_feet.y = m_groundY;
                m_velocityY = 0.0f;
                m_airborne = false;
                m_landingLeft = LandingSeconds;
            }
            return;
        }

        if (m_landingLeft > 0.0f) {
            m_landingLeft -= deltaSeconds;
        }

        if (direction == 0) {
            // Every stop starts a fresh breath, never halfway through the tail swish.
            if (m_walking) {
                m_idle.stop(0);
                m_walking = false;
            }
            m_walk.stop(IdleFrame);
            m_idle.update(deltaSeconds);
        } else {
            m_walking = true;
            m_walk.update(deltaSeconds);
        }
    }

    void Player::jump()
    {
        if (m_airborne) {
            return;
        }

        m_airborne = true;
        m_velocityY = -JumpSpeed;
        m_landingLeft = 0.0f;
    }

    void Player::stand()
    {
        m_feet.y = m_groundY;
        m_velocityY = 0.0f;
        m_airborne = false;
        m_landingLeft = 0.0f;
        m_walking = false;
        m_walk.stop(IdleFrame);
        m_idle.stop(0);
    }

    gfx::SpriteDraw Player::sprite() const
    {
        gfx::SpriteDraw draw;
        draw.texture = m_walkSheet;
        draw.source = m_walk.sourceRect();

        if (!m_walking && m_idleSheet != gfx::TextureHandle::Invalid) {
            draw.texture = m_idleSheet;
            draw.source = m_idle.sourceRect();
        }

        if (m_airborne || m_landingLeft > 0.0f) {
            const float frame = m_airborne ? (m_velocityY < 0.0f ? RisingFrame : FallingFrame) : LandingFrame;
            draw.texture = m_jumpSheet;
            draw.source = gfx::Rect{ frame * FrameSize, 0.0f, FrameSize, FrameSize };
        }

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
