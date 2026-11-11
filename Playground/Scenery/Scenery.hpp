/*
    Created on: 11/9/26
    Filename: Scenery.hpp
    Description: The level's backdrop (sky, sun, hills) and the bushes the player walks behind.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#ifndef RENDERER_WRAPPER_SCENERY_HPP
#define RENDERER_WRAPPER_SCENERY_HPP

#include "Renderer/IRenderer.hpp"

#include <vector>

namespace playground {

    class Scenery {
        public:
            explicit Scenery(float groundTop);

            void drawBackground(gfx::IRenderer& renderer, gfx::Vec2 viewport) const;
            void drawForeground(gfx::IRenderer& renderer) const;

        private:
            float m_groundTop;
            std::vector<gfx::Vec2> m_farHills;
            std::vector<gfx::Vec2> m_nearHills;
    };

}

#endif //RENDERER_WRAPPER_SCENERY_HPP
