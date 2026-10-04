/*
    Created on: 10/4/26
    Filename: Player.hpp
    Description: Playground bull: walks left/right on the ground, animated from its sheet.

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
            // feet is the world point under the hooves, horizontally centred on the bull.
            Player(gfx::TextureHandle sheet, gfx::Vec2 feet);

            // Reads the keyboard directly: playground-only, the engine will feed input instead.
            void update(float deltaSeconds);

            // Freezes on the standing frame, e.g. when the playground leaves play mode mid-stride.
            void stand();

            gfx::SpriteDraw sprite() const;
            gfx::Vec2 position() const;

        private:
            gfx::TextureHandle m_sheet;
            gfx::Vec2 m_feet;
            bool m_facingLeft = false;
            gfx::SpriteAnimation m_walk;
    };

}

#endif //RENDERER_WRAPPER_PLAYER_HPP
