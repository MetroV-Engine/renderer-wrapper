/*
    Created on: 9/22/26
    Filename: NullRenderer.cpp
    Description: Headless backend: records draw calls so systems can be tested without a window.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/NullRenderer/NullRenderer.hpp"

namespace gfx {

    NullRenderer::NullRenderer(const uint32_t width, const uint32_t height)
        : m_width(width)
        , m_height(height)
    {
        m_camera.center = Vec2{ static_cast<float>(width) * 0.5f, static_cast<float>(height) * 0.5f };
    }

    void NullRenderer::resize(const uint32_t width, const uint32_t height)
    {
        if (width == 0 || height == 0) {
            return;
        }

        m_width = width;
        m_height = height;
    }

    void NullRenderer::beginFrame(const Color clear)
    {
        m_lastClear = clear;
        m_inFrame = true;
    }

    void NullRenderer::endFrame()
    {
        m_inFrame = false;
        ++m_framesPresented;
    }

    void NullRenderer::setCamera(const Camera2D& camera)
    {
        m_camera = camera;
    }

    TextureHandle NullRenderer::createTexture(const uint8_t* rgba, const uint32_t width, const uint32_t height)
    {
        if (rgba == nullptr || width == 0 || height == 0) {
            return TextureHandle::Invalid;
        }

        const uint32_t id = m_nextTexture++;
        m_textures.emplace(id, Vec2{ static_cast<float>(width), static_cast<float>(height) });

        return static_cast<TextureHandle>(id);
    }

    void NullRenderer::destroyTexture(const TextureHandle texture)
    {
        m_textures.erase(static_cast<uint32_t>(texture));
    }

    Vec2 NullRenderer::textureSize(const TextureHandle texture) const
    {
        const auto found = m_textures.find(static_cast<uint32_t>(texture));

        return found != m_textures.end() ? found->second : Vec2{ 0.0f, 0.0f };
    }

    void NullRenderer::drawRect(const RectDraw& draw)
    {
        if (!m_inFrame) {
            return;
        }

        Recorded entry;
        entry.kind = Recorded::Kind::Rect;
        entry.rect = draw;
        m_recorded.push_back(entry);
    }

    void NullRenderer::drawSprite(const SpriteDraw& draw)
    {
        if (!m_inFrame) {
            return;
        }

        Recorded entry;
        entry.kind = Recorded::Kind::Sprite;
        entry.sprite = draw;
        m_recorded.push_back(entry);
    }

    const char* NullRenderer::backendName() const
    {
        return "null";
    }

}
