/*
    Created on: 8/29/26
    Filename: main.cpp
    Description: SDL3 playground: pan and zoom over a few rectangles through IRenderer.

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
#include <vector>

#include "Renderer/RendererFactory/RendererFactory.hpp"

namespace {

    constexpr uint32_t WindowWidth = 1280;
    constexpr uint32_t WindowHeight = 720;

    constexpr float PanSpeed = 700.0f;      // world units per second at zoom 1
    constexpr float ZoomStep = 1.12f;
    constexpr float MinZoom = 0.2f;
    constexpr float MaxZoom = 8.0f;

    constexpr gfx::Color Background{ 24, 26, 32, 255 };
    constexpr gfx::Color Ground{ 60, 72, 58, 255 };
    constexpr gfx::Color Platform{ 92, 104, 128, 255 };
    constexpr gfx::Color Player{ 226, 122, 96, 255 };
    constexpr gfx::Color Hud{ 255, 255, 255, 40 };

    // Wide enough that panning has somewhere to go.
    std::vector<gfx::RectDraw> makeScene()
    {
        std::vector<gfx::RectDraw> scene;

        scene.push_back({ gfx::Rect{ -2000.0f, 600.0f, 6000.0f, 120.0f }, Ground, 0, gfx::Space::World });

        for (int i = 0; i < 8; ++i) {
            const auto x = static_cast<float>(-800 + i * 320);
            const auto y = static_cast<float>(420 - (i % 4) * 90);
            scene.push_back({ gfx::Rect{ x, y, 180.0f, 28.0f }, Platform, 1, gfx::Space::World });
        }

        scene.push_back({ gfx::Rect{ 40.0f, 520.0f, 48.0f, 80.0f }, Player, 2, gfx::Space::World });

        return scene;
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
    (void)argc;
    (void)argv;

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

    std::unique_ptr<gfx::IRenderer> renderer =
        gfx::makeRenderer(gfx::Backend::Sdl3Cpu, window, WindowWidth, WindowHeight);
    if (renderer == nullptr) {
        std::printf("Renderer creation failed\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::printf("backend: %s\n", renderer->backendName());
    std::printf("arrows or WASD pan | mouse wheel zooms | escape quits\n");

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
                    zoom(camera, event.wheel.y > 0.0f ? ZoomStep : 1.0f / ZoomStep);
                    break;

                case SDL_EVENT_KEY_DOWN:
                    if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                        running = false;
                    }
                    break;

                default:
                    break;
            }
        }

        panWithKeyboard(camera, deltaSeconds);

        renderer->beginFrame(Background);
        renderer->setCamera(camera);

        for (const gfx::RectDraw& object : scene) {
            renderer->drawRect(object);
        }

        // Screen space ignores the camera: this bar stays put while the world scrolls under it.
        renderer->drawRect({ gfx::Rect{ 0.0f, 0.0f, viewport.x, 36.0f }, Hud, 10, gfx::Space::Screen });

        renderer->endFrame();
    }

    // The renderer holds an SDL_Renderer tied to this window: release it before the window dies.
    renderer.reset();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
