/*
    Created on: 8/29/26
    Filename: main.cpp
    Description: SDL3 playground: edit mode pans the scene, play mode walks the bull (Tab or HUD button).

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include <SDL3/SDL.h>

#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Tools/ImageLoader/ImageLoader.hpp"
#include "Playground/ModeToggle/ModeToggle.hpp"
#include "Playground/Player/Player.hpp"
#include "Playground/Showcase/Showcase.hpp"
#include "Renderer/RendererFactory/RendererFactory.hpp"

namespace {

    constexpr uint32_t WindowWidth = 1280;
    constexpr uint32_t WindowHeight = 720;

    constexpr float GroundTop = 600.0f;
    constexpr float PlayFocusAbove = 240.0f;    // camera centre above the bull's feet, as at startup
    constexpr gfx::Vec2 ShowcaseCenter{ -70.0f, 170.0f };   // above the third platform

    constexpr float PanSpeed = 700.0f;      // world units per second at zoom 1
    constexpr float ZoomStep = 1.12f;
    constexpr float MinZoom = 0.2f;
    constexpr float MaxZoom = 8.0f;

    constexpr gfx::Color Background{ 24, 26, 32, 255 };
    constexpr gfx::Color Ground{ 60, 72, 58, 255 };
    constexpr gfx::Color Platform{ 92, 104, 128, 255 };
    constexpr gfx::Color Hud{ 255, 255, 255, 40 };

    // Wide enough that panning has somewhere to go.
    std::vector<gfx::RectDraw> makeScene()
    {
        std::vector<gfx::RectDraw> scene;

        scene.push_back({ gfx::Rect{ -2000.0f, GroundTop, 6000.0f, 120.0f }, Ground, 0, gfx::Space::World });

        for (int i = 0; i < 8; ++i) {
            const auto x = static_cast<float>(-800 + i * 320);
            const auto y = static_cast<float>(420 - (i % 4) * 90);
            scene.push_back({ gfx::Rect{ x, y, 180.0f, 28.0f }, Platform, 1, gfx::Space::World });
        }

        return scene;
    }

    // CLion runs from the build directory: resolve next to the binary, where CMake copies assets/.
    std::string assetPath(const char* relative)
    {
        const char* base = SDL_GetBasePath();

        return std::string(base != nullptr ? base : "") + "assets/" + relative;
    }

    // The play button runs bgfx; a second run configuration passes --backend=sdl.
    std::optional<gfx::Backend> parseBackend(const int argc, char** argv)
    {
        gfx::Backend backend = gfx::Backend::Bgfx;

        for (int i = 1; i < argc; ++i) {
            const std::string_view argument = argv[i];
            if (argument == "--backend=bgfx") {
                backend = gfx::Backend::Bgfx;
            } else if (argument == "--backend=sdl") {
                backend = gfx::Backend::Sdl3Cpu;
            } else {
                std::printf("unknown argument: %s\nusage: renderer_wrapper [--backend=bgfx|sdl]\n", argv[i]);
                return std::nullopt;
            }
        }

        return backend;
    }

    void panWithKeyboard(gfx::Camera2D& camera, const float deltaSeconds)
    {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        const float step = PanSpeed * deltaSeconds / camera.zoom;   // constant speed on screen

        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
            camera.center.x -= step;
        }
        if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
            camera.center.x += step;
        }
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
            camera.center.y -= step;
        }
        if (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) {
            camera.center.y += step;
        }
    }

    void zoom(gfx::Camera2D& camera, const float factor)
    {
        camera.zoom *= factor;
        camera.zoom = camera.zoom < MinZoom ? MinZoom : (camera.zoom > MaxZoom ? MaxZoom : camera.zoom);
    }

}

int main(int argc, char** argv)
{
    const std::optional<gfx::Backend> backend = parseBackend(argc, argv);
    if (!backend.has_value()) {
        return 1;
    }

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("renderer-wrapper playground",
        static_cast<int>(WindowWidth), static_cast<int>(WindowHeight), SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        std::printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    std::unique_ptr<gfx::IRenderer> renderer = gfx::makeRenderer(*backend, window, WindowWidth, WindowHeight);
    if (renderer == nullptr) {
        std::printf("Renderer creation failed (try --backend=sdl)\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::printf("backend: %s\n", renderer->backendName());
    std::printf("Tab or HUD button toggles edit/play | edit: WASD/arrows pan | play: A/D/arrows walk\n");
    std::printf("edit: mouse wheel zooms | escape quits\n");
    std::printf("checks: R reloads the showcase texture mid-frame | F forces renderer misuse warnings\n");

    const std::optional<tools::Image> sheet = tools::loadPng(assetPath("bongo_marche_spritesheet.png"));
    const gfx::TextureHandle bullTexture = sheet.has_value()
        ? renderer->createTexture(sheet->rgba.data(), sheet->width, sheet->height)
        : gfx::TextureHandle::Invalid;
    if (bullTexture == gfx::TextureHandle::Invalid) {
        std::printf("Could not load the bull sprite sheet\n");
        renderer.reset();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    playground::Player player(bullTexture, gfx::Vec2{ 120.0f, GroundTop });
    playground::ModeToggle modeToggle(playground::Mode::Edit);

    // Holds a texture on the renderer: optional so it can be released before the renderer dies.
    std::optional<playground::Showcase> showcase;
    showcase.emplace(*renderer, *sheet, ShowcaseCenter);

    const std::vector<gfx::RectDraw> scene = makeScene();

    gfx::Vec2 viewport{ static_cast<float>(WindowWidth), static_cast<float>(WindowHeight) };
    gfx::Camera2D camera;
    camera.center = gfx::Vec2{ viewport.x * 0.5f, viewport.y * 0.5f };

    uint64_t previousTicks = SDL_GetTicks();
    bool running = true;

    while (running) {
        const uint64_t now = SDL_GetTicks();
        const float deltaSeconds = static_cast<float>(now - previousTicks) / 1000.0f;
        previousTicks = now;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (modeToggle.handleEvent(event)) {
                if (modeToggle.mode() == playground::Mode::Play) {
                    camera.center = gfx::Vec2{ player.position().x, player.position().y - PlayFocusAbove };
                    camera.zoom = 1.0f;     // play mode cannot zoom, so do not inherit the editor's zoom
                } else {
                    player.stand();
                }
            }

            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    viewport = gfx::Vec2{ static_cast<float>(event.window.data1), static_cast<float>(event.window.data2) };
                    renderer->resize(static_cast<uint32_t>(event.window.data1),
                                     static_cast<uint32_t>(event.window.data2));
                    break;

                case SDL_EVENT_MOUSE_WHEEL:
                    if (modeToggle.mode() == playground::Mode::Edit) {
                        zoom(camera, event.wheel.y > 0.0f ? ZoomStep : 1.0f / ZoomStep);
                    }
                    break;

                case SDL_EVENT_KEY_DOWN:
                    if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                        running = false;
                    } else if (event.key.repeat) {
                        break;
                    } else if (event.key.scancode == SDL_SCANCODE_R) {
                        showcase->requestReload();
                    } else if (event.key.scancode == SDL_SCANCODE_F) {
                        showcase->misuse();
                    }
                    break;

                default:
                    break;
            }
        }

        if (modeToggle.mode() == playground::Mode::Play) {
            player.update(deltaSeconds);
            camera.center.x = player.position().x;
        } else {
            panWithKeyboard(camera, deltaSeconds);
        }
        showcase->update(deltaSeconds);

        renderer->beginFrame(Background);
        renderer->setCamera(camera);

        for (const gfx::RectDraw& object : scene) {
            renderer->drawRect(object);
        }

        renderer->drawSprite(player.sprite());
        showcase->draw(viewport);

        // Screen space ignores the camera: this bar stays put while the world scrolls under it.
        renderer->drawRect({ gfx::Rect{ 0.0f, 0.0f, viewport.x, 36.0f }, Hud, 10, gfx::Space::Screen });
        modeToggle.draw(*renderer);

        renderer->endFrame();
    }

    showcase.reset();
    renderer->destroyTexture(bullTexture);

    // The renderer holds an SDL_Renderer tied to this window: release it before the window dies.
    renderer.reset();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
