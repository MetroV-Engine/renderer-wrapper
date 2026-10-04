/*
    Created on: 9/22/26
    Filename: SdlRenderer.cpp
    Description: SDL3 backend: the interim renderer the UI team builds against.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/SdlRenderer/SdlRenderer.hpp"

#include "Renderer/Transform/Transform.hpp"

#include <SDL3/SDL.h>

#include <iostream>
#include <numbers>

namespace gfx {

    namespace {
        SDL_FRect toSdl(const Rect& rect)
        {
            return SDL_FRect{ rect.x, rect.y, rect.w, rect.h };
        }

        double toDegrees(const float radians)
        {
            return static_cast<double>(radians) * 180.0 / std::numbers::pi;
        }
    }

    std::unique_ptr<SdlRenderer> SdlRenderer::create(SDL_Window* window, const uint32_t width, const uint32_t height)
    {
        if (window == nullptr) {
            std::cerr << "[SdlRenderer] ERROR: null window\n";
            return nullptr;
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
        if (renderer == nullptr) {
            std::cerr << "[SdlRenderer] ERROR: SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
            return nullptr;
        }

        // Rect alpha is meaningless without this; SDL defaults draws to no blending.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        return std::unique_ptr<SdlRenderer>(new SdlRenderer(renderer, width, height));
    }

    SdlRenderer::SdlRenderer(SDL_Renderer* renderer, const uint32_t width, const uint32_t height)
        : m_renderer(renderer)
        , m_width(width)
        , m_height(height)
    {
        const char* driver = SDL_GetRendererName(m_renderer);
        m_name = std::string("SDL3 / ") + (driver != nullptr ? driver : "unknown");

        // World coordinates then match screen pixels, as the ECS prototype expects.
        m_camera.center = Vec2{ static_cast<float>(width) * 0.5f, static_cast<float>(height) * 0.5f };

        std::cerr << "[SdlRenderer] ready: " << m_name << " " << width << "x" << height << "\n";
    }

    SdlRenderer::~SdlRenderer()
    {
        for (const auto& [id, texture] : m_textures) {
            SDL_DestroyTexture(texture);
        }
        m_textures.clear();

        if (m_renderer != nullptr) {
            SDL_DestroyRenderer(m_renderer);
            m_renderer = nullptr;
        }
    }

    void SdlRenderer::resize(const uint32_t width, const uint32_t height)
    {
        if (width == 0 || height == 0) {
            return;
        }

        m_width = width;
        m_height = height;
    }

    void SdlRenderer::beginFrame(const Color clear)
    {
        if (m_inFrame) {
            std::cerr << "[SdlRenderer] WARN: beginFrame() called twice; ignoring\n";
            return;
        }

        for (auto& layer : m_layers) {
            layer.clear();
        }

        SDL_SetRenderDrawColor(m_renderer, clear.r, clear.g, clear.b, clear.a);
        SDL_RenderClear(m_renderer);
        m_inFrame = true;
    }

    void SdlRenderer::endFrame()
    {
        if (!m_inFrame) {
            std::cerr << "[SdlRenderer] WARN: endFrame() without beginFrame(); ignoring\n";
            return;
        }

        for (const auto& layer : m_layers) {
            for (const Command& command : layer) {
                flush(command);
            }
        }

        SDL_RenderPresent(m_renderer);
        m_inFrame = false;
        releasePending();
    }

    void SdlRenderer::setCamera(const Camera2D& camera)
    {
        m_camera = camera;
    }

    TextureHandle SdlRenderer::createTexture(const uint8_t* rgba, const uint32_t width, const uint32_t height)
    {
        if (rgba == nullptr || width == 0 || height == 0) {
            std::cerr << "[SdlRenderer] ERROR: createTexture called with empty pixels\n";
            return TextureHandle::Invalid;
        }

        SDL_Texture* texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA32,
            SDL_TEXTUREACCESS_STATIC, static_cast<int>(width), static_cast<int>(height));
        if (texture == nullptr) {
            std::cerr << "[SdlRenderer] ERROR: SDL_CreateTexture failed: " << SDL_GetError() << "\n";
            return TextureHandle::Invalid;
        }

        // UpdateTexture copies, so the caller keeps ownership of its pixels.
        if (!SDL_UpdateTexture(texture, nullptr, rgba, static_cast<int>(width * 4))) {
            std::cerr << "[SdlRenderer] ERROR: SDL_UpdateTexture failed: " << SDL_GetError() << "\n";
            SDL_DestroyTexture(texture);
            return TextureHandle::Invalid;
        }

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

        const uint32_t id = m_nextTexture++;
        m_textures.emplace(id, texture);

        return static_cast<TextureHandle>(id);
    }

    void SdlRenderer::destroyTexture(const TextureHandle texture)
    {
        if (find(texture) == nullptr) {
            if (!m_warnedInvalidTexture) {
                std::cerr << "[SdlRenderer] WARN: destroyTexture on an unknown handle\n";
                m_warnedInvalidTexture = true;
            }
            return;
        }

        // Draws are queued until endFrame(), so the pixels must outlive the frame that uses them.
        if (m_inFrame) {
            m_pendingDestroy.push_back(texture);
            return;
        }

        const auto id = static_cast<uint32_t>(texture);
        SDL_DestroyTexture(m_textures[id]);
        m_textures.erase(id);
    }

    Vec2 SdlRenderer::textureSize(const TextureHandle texture) const
    {
        SDL_Texture* found = find(texture);
        if (found == nullptr) {
            return Vec2{ 0.0f, 0.0f };
        }

        Vec2 size;
        SDL_GetTextureSize(found, &size.x, &size.y);

        return size;
    }

    void SdlRenderer::drawRect(const RectDraw& draw)
    {
        Command command;
        command.kind = Command::Kind::Rect;
        command.dest = draw.dest;
        command.color = draw.color;
        command.space = draw.space;

        submit(command, draw.layer);
    }

    void SdlRenderer::drawSprite(const SpriteDraw& draw)
    {
        if (find(draw.texture) == nullptr) {
            if (!m_warnedInvalidTexture) {
                std::cerr << "[SdlRenderer] WARN: drawSprite with an invalid texture handle\n";
                m_warnedInvalidTexture = true;
            }
            return;
        }

        Command command;
        command.kind = Command::Kind::Sprite;
        command.texture = draw.texture;
        command.source = draw.source;
        command.dest = draw.dest;
        command.rotation = draw.rotation;
        command.flipX = draw.flipX;
        command.color = draw.tint;
        command.space = draw.space;

        submit(command, draw.layer);
    }

    const char* SdlRenderer::backendName() const
    {
        return m_name.c_str();
    }

    void SdlRenderer::submit(const Command& command, const Layer layer)
    {
        if (!m_inFrame) {
            if (!m_warnedOutsideFrame) {
                std::cerr << "[SdlRenderer] WARN: draw outside beginFrame()/endFrame(); ignoring\n";
                m_warnedOutsideFrame = true;
            }
            return;
        }

        Layer clamped = layer;
        if (clamped < MinLayer || clamped > MaxLayer) {
            if (!m_warnedLayerRange) {
                std::cerr << "[SdlRenderer] WARN: layer " << layer << " out of range, clamping\n";
                m_warnedLayerRange = true;
            }
            clamped = clamped < MinLayer ? MinLayer : MaxLayer;
        }

        m_layers[static_cast<size_t>(clamped)].push_back(command);
    }

    void SdlRenderer::flush(const Command& command) const
    {
        const Vec2 viewport{ static_cast<float>(m_width), static_cast<float>(m_height) };
        const Rect dest = command.space == Space::World
            ? worldToScreen(m_camera, command.dest, viewport)
            : command.dest;
        const SDL_FRect destination = toSdl(dest);

        if (command.kind == Command::Kind::Rect) {
            SDL_SetRenderDrawColor(m_renderer, command.color.r, command.color.g, command.color.b, command.color.a);
            SDL_RenderFillRect(m_renderer, &destination);
            return;
        }

        SDL_Texture* texture = find(command.texture);
        if (texture == nullptr) {
            return;
        }

        SDL_SetTextureColorMod(texture, command.color.r, command.color.g, command.color.b);
        SDL_SetTextureAlphaMod(texture, command.color.a);

        const SDL_FRect source = toSdl(command.source);
        const bool wholeTexture = command.source.w <= 0.0f || command.source.h <= 0.0f;

        SDL_RenderTextureRotated(m_renderer, texture, wholeTexture ? nullptr : &source, &destination,
            toDegrees(command.rotation), nullptr, command.flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }

    SDL_Texture* SdlRenderer::find(const TextureHandle texture) const
    {
        const auto found = m_textures.find(static_cast<uint32_t>(texture));

        return found != m_textures.end() ? found->second : nullptr;
    }

    void SdlRenderer::releasePending()
    {
        for (const TextureHandle texture : m_pendingDestroy) {
            const auto id = static_cast<uint32_t>(texture);
            const auto found = m_textures.find(id);
            if (found != m_textures.end()) {
                SDL_DestroyTexture(found->second);
                m_textures.erase(found);
            }
        }

        m_pendingDestroy.clear();
    }

}
