/*
    Created on: 9/22/26
    Filename: SdlWindowBridge.hpp
    Description: Extracts native window handles from an SDL_Window for backends that need them.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#ifndef RENDERER_WRAPPER_SDLWINDOWBRIDGE_HPP
#define RENDERER_WRAPPER_SDLWINDOWBRIDGE_HPP

#include <cstdint>

struct SDL_Window;

namespace gfx {

    enum class NativeWindowKind : uint8_t { Unsupported, X11, Wayland, Win32, Cocoa };

    struct NativeWindow {
        void* nwh = nullptr;                                   // window / surface
        void* ndt = nullptr;                                   // display, *nix only
        NativeWindowKind kind = NativeWindowKind::Unsupported;
    };

    // nwh is null when the handles could not be obtained; the reason is logged.
    NativeWindow nativeWindowFromSdl(SDL_Window* window);

}

#endif //RENDERER_WRAPPER_SDLWINDOWBRIDGE_HPP
