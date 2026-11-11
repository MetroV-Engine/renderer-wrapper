/*
    Created on: 10/4/26
    Filename: Showcase.hpp
    Description: Exercises the IRenderer paths the bull does not reach.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#ifndef RENDERER_WRAPPER_SHOWCASE_HPP
#define RENDERER_WRAPPER_SHOWCASE_HPP

#include "Renderer/IRenderer.hpp"
#include "Tools/ImageLoader/ImageLoader.hpp"

namespace playground {

    // Rotation, tint, screen-space and whole-texture sprites, mid-frame reload, misuse warnings.
    class Showcase {
        public:
            // Both references must outlive the Showcase: reload re-uploads the sheet's pixels.
            Showcase(gfx::IRenderer& renderer, const tools::Image& sheet, gfx::Vec2 center);
            ~Showcase();

            Showcase(const Showcase&) = delete;
            Showcase& operator=(const Showcase&) = delete;

            void update(float deltaSeconds);

            // Inside beginFrame()/endFrame(): a requested reload runs after the draws are queued.
            void draw(gfx::Vec2 viewport);

            void requestReload();

            // Between frames only: it opens and closes frames itself to trigger each warning.
            void misuse();

        private:
            gfx::IRenderer& m_renderer;
            const tools::Image& m_sheet;
            gfx::TextureHandle m_texture = gfx::TextureHandle::Invalid;
            gfx::Vec2 m_sheetSize;
            gfx::Vec2 m_center;
            float m_time = 0.0f;
            bool m_reloadRequested = false;
    };

}

#endif //RENDERER_WRAPPER_SHOWCASE_HPP
