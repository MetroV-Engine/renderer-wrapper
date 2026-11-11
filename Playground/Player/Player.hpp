/*
    Created on: 10/4/26
    Filename: Player.hpp
    Description: Playground bull: idles, walks left/right and hops on the ground, animated from its sheets.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#ifndef RENDERER_WRAPPER_PLAYER_HPP
#define RENDERER_WRAPPER_PLAYER_HPP

#include "Renderer/RendererTypes.hpp"
#include "Renderer/SpriteAnimation/SpriteAnimation.hpp"

namespace playground {

    class Player {
        public:
            // feet is the world point under the hooves, horizontally centred on the bull; its y is the ground.
            // idleSheet loops twelve frames; Invalid stands still on a walk frame instead.
            // jumpSheet holds three frames: rising, falling, landing.
            Player(gfx::TextureHandle walkSheet, gfx::TextureHandle idleSheet, gfx::TextureHandle jumpSheet, gfx::Vec2 feet);

            // Reads the keyboard directly: playground-only, the engine will feed input instead.
            void update(float deltaSeconds);

            // Ignored while already in the air.
            void jump();

            // Back on the ground on the standing frame, e.g. when the playground leaves play mode mid-jump.
            void stand();

            gfx::SpriteDraw sprite() const;
            gfx::Vec2 position() const;

        private:
            gfx::TextureHandle m_walkSheet;
            gfx::TextureHandle m_idleSheet;
            gfx::TextureHandle m_jumpSheet;
            gfx::Vec2 m_feet;
            float m_groundY;
            float m_velocityY = 0.0f;       // world units per second, positive is down
            float m_landingLeft = 0.0f;     // seconds the landing frame still shows
            bool m_airborne = false;
            bool m_walking = false;
            bool m_facingLeft = false;
            gfx::SpriteAnimation m_walk;
            gfx::SpriteAnimation m_idle;
    };

}

#endif //RENDERER_WRAPPER_PLAYER_HPP
