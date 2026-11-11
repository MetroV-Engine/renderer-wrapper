/*
    Created on: 10/4/26
    Filename: ModeToggle.hpp
    Description: HUD button and Tab key switching the playground between edit and play.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#ifndef RENDERER_WRAPPER_MODETOGGLE_HPP
#define RENDERER_WRAPPER_MODETOGGLE_HPP

#include "Renderer/IRenderer.hpp"

#include <cstdint>

union SDL_Event;

namespace playground {

    enum class Mode : uint8_t { Edit, Play };

    class ModeToggle {
        public:
            explicit ModeToggle(Mode initial);

            // True when this event flipped the mode.
            bool handleEvent(const SDL_Event& event);

            Mode mode() const;

            // Screen space, above the HUD bar: green with a play arrow, or blue with pause bars.
            void draw(gfx::IRenderer& renderer) const;

        private:
            void toggle();

            Mode m_mode;
    };

}

#endif //RENDERER_WRAPPER_MODETOGGLE_HPP
