/*
    Created on: 9/22/26
    Filename: SdlRenderer.hpp
    Description: SDL3 backend: the interim renderer the UI team builds against.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#ifndef RENDERER_WRAPPER_SDLRENDERER_HPP
#define RENDERER_WRAPPER_SDLRENDERER_HPP

#include "Renderer/IRenderer.hpp"

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace gfx {

    class SdlRenderer final : public IRenderer {
        public:
            // Returns nullptr on failure, after logging the reason.
            static std::unique_ptr<SdlRenderer> create(SDL_Window* window, uint32_t width, uint32_t height);

            ~SdlRenderer() override;

            void resize(uint32_t width, uint32_t height) override;

            void beginFrame(Color clear) override;
            void endFrame() override;

            void setCamera(const Camera2D& camera) override;

            TextureHandle createTexture(const uint8_t* rgba, uint32_t width, uint32_t height) override;
            void destroyTexture(TextureHandle texture) override;
            Vec2 textureSize(TextureHandle texture) const override;

            void drawRect(const RectDraw& draw) override;
            void drawSprite(const SpriteDraw& draw) override;

            const char* backendName() const override;

        private:
            struct Command {
                enum class Kind : uint8_t { Rect, Sprite };

                Kind kind = Kind::Rect;
                TextureHandle texture = TextureHandle::Invalid;
                Rect source;
                Rect dest;
                float rotation = 0.0f;
                bool flipX = false;
                Color color;
                Space space = Space::World;
            };

            SdlRenderer(SDL_Renderer* renderer, uint32_t width, uint32_t height);

            void submit(const Command& command, Layer layer);
            void flush(const Command& command) const;
            SDL_Texture* find(TextureHandle texture) const;
            void releasePending();

            SDL_Renderer* m_renderer = nullptr;
            std::string m_name;

            uint32_t m_width = 0;
            uint32_t m_height = 0;
            Camera2D m_camera;
            bool m_inFrame = false;

            std::array<std::vector<Command>, LayerCount> m_layers{};
            std::unordered_map<uint32_t, SDL_Texture*> m_textures;
            uint32_t m_nextTexture = 1;
            std::vector<TextureHandle> m_pendingDestroy;

            bool m_warnedOutsideFrame = false;
            bool m_warnedLayerRange = false;
            bool m_warnedInvalidTexture = false;
    };

}

#endif //RENDERER_WRAPPER_SDLRENDERER_HPP
