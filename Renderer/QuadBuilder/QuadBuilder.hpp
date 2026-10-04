/*
    Created on: 10/4/26
    Filename: QuadBuilder.hpp
    Description: Turns a draw into four GPU-ready vertices: backend-free.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#ifndef RENDERER_WRAPPER_QUADBUILDER_HPP
#define RENDERER_WRAPPER_QUADBUILDER_HPP

#include "Renderer/RendererTypes.hpp"

#include <array>
#include <cstdint>

namespace gfx {

    // Field order and sizes are the bgfx vertex layout: Position 2f, TexCoord0 2f, Color0 4u8.
    struct QuadVertex {
        float x = 0.0f;
        float y = 0.0f;
        float u = 0.0f;
        float v = 0.0f;
        uint8_t r = 255;
        uint8_t g = 255;
        uint8_t b = 255;
        uint8_t a = 255;
    };
    static_assert(sizeof(QuadVertex) == 20, "QuadVertex must match the 20-byte bgfx layout");

    // Corners TL, TR, BR, BL of dest before rotation; positions stay in the draw's space.
    using Quad = std::array<QuadVertex, 4>;

    inline constexpr std::array<uint16_t, 6> QuadIndices{ 0, 1, 2, 0, 2, 3 };

    Quad buildQuad(const SpriteDraw& draw, Vec2 textureSize);

    // UVs span 0..1 so the quad samples the renderer's 1x1 white texture.
    Quad buildQuad(const RectDraw& draw);

}

#endif //RENDERER_WRAPPER_QUADBUILDER_HPP
