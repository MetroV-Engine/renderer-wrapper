/*
    Created on: 11/11/26
    Filename: Gallery.hpp
    Description: One labelled sample of every shape call, side by side in the level.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#ifndef RENDERER_WRAPPER_GALLERY_HPP
#define RENDERER_WRAPPER_GALLERY_HPP

#include "Renderer/IRenderer.hpp"

#include <array>

namespace playground {

    // Line, triangle, quad, circle, polygon, textured mesh and world-space text in one world-space row.
    class Gallery {
        public:
            // A null font draws the row without labels; the font must outlive the last draw().
            Gallery(const gfx::IRenderer& renderer, gfx::TextureHandle sheet, const gfx::Font* font, gfx::Vec2 origin);

            // Inside beginFrame()/endFrame(): a requested misuse runs after the samples are queued.
            void draw(gfx::IRenderer& renderer);

            // Degenerate polygon, mesh and text calls: each warns once per run or draws nothing.
            void requestMisuse();

        private:
            void drawSamples(gfx::IRenderer& renderer) const;
            void drawLabels(gfx::IRenderer& renderer) const;
            void drawMisuse(gfx::IRenderer& renderer) const;

            gfx::TextureHandle m_sheet;
            const gfx::Font* m_font;
            gfx::Vec2 m_origin;
            gfx::Vec2 m_frameUv;        // far corner of the sheet's first frame
            std::array<gfx::Vec2, 10> m_star;
            bool m_misuseRequested = false;
    };

}

#endif //RENDERER_WRAPPER_GALLERY_HPP
