/*
    Created on: 9/22/26
    Filename: RendererTypes.hpp
    Description: Shared POD types for the rendering interface: no backend headers.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#ifndef RENDERER_WRAPPER_RENDERERTYPES_HPP
#define RENDERER_WRAPPER_RENDERERTYPES_HPP

#include <cstdint>

namespace gfx {

    struct Vec2 {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;
    };

    struct Color {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t a = 255;
    };

    inline constexpr Color White{ 255, 255, 255, 255 };
    inline constexpr Color Black{ 0, 0, 0, 255 };

    // center is the world point shown at the middle of the viewport; zoom > 1 magnifies.
    struct Camera2D {
        Vec2 center;
        float zoom = 1.0f;
    };

    enum class TextureHandle : uint32_t { Invalid = 0 };

    enum class Space : uint8_t { World, Screen };

    using Layer = int;
    inline constexpr Layer MinLayer = 0;
    inline constexpr Layer MaxLayer = 63;
    inline constexpr int LayerCount = MaxLayer - MinLayer + 1;

    struct RectDraw {
        Rect dest;
        Color color;
        Layer layer = MinLayer;
        Space space = Space::World;
    };

    struct SpriteDraw {
        TextureHandle texture = TextureHandle::Invalid;
        Rect source;                // w or h <= 0 means the whole texture
        Rect dest;
        float rotation = 0.0f;      // radians, clockwise, about the centre of dest
        bool flipX = false;         // mirrors the source horizontally inside dest
        Layer layer = MinLayer;
        Space space = Space::World;
        Color tint = White;
    };

}

#endif //RENDERER_WRAPPER_RENDERERTYPES_HPP
