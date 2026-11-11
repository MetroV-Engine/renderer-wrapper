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
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Tools/ImageLoader/ImageLoader.hpp"
#include "Playground/Gallery/Gallery.hpp"
#include "Playground/ModeToggle/ModeToggle.hpp"
#include "Playground/Player/Player.hpp"
#include "Playground/Scenery/Scenery.hpp"
#include "Playground/Showcase/Showcase.hpp"
#include "Renderer/Colors/Colors.hpp"
#include "Renderer/Font/Font.hpp"
#include "Renderer/RendererFactory/RendererFactory.hpp"

namespace {

    constexpr uint32_t WindowWidth = 1280;
    constexpr uint32_t WindowHeight = 720;

    constexpr float GroundTop = 600.0f;
    constexpr float PlayFocusAbove = 240.0f;    // camera centre above the bull's feet, as at startup
    constexpr gfx::Vec2 ShowcaseCenter{ -70.0f, 170.0f };   // above the third platform
    constexpr gfx::Vec2 GalleryOrigin{ 130.0f, 80.0f };     // first sample's centre: the row is on screen at startup

    constexpr float PanSpeed = 700.0f;      // world units per second at zoom 1
    constexpr float ZoomStep = 1.12f;
    constexpr float MinZoom = 0.2f;
    constexpr float MaxZoom = 8.0f;
    constexpr float FontPixelHeight = 24.0f;
    constexpr float HudHeight = 36.0f;
    constexpr float HudTextX = 40.0f;           // right of the mode button
    constexpr float EdgeThickness = 3.0f;
    constexpr float EdgeLightness = 0.35f;

    constexpr gfx::Color Background = gfx::fromHex(0x181A20FF);
    constexpr gfx::Color Ground = gfx::fromHex(0x3C483AFF);
    constexpr gfx::Color Platform = gfx::fromHex(0x5C6880FF);
    constexpr gfx::Color Hud = gfx::withAlpha(gfx::White, 40);

    // Wide enough that panning has somewhere to go.
    std::vector<gfx::RectDraw> makeScene()
    {
        std::vector<gfx::RectDraw> scene;

        scene.push_back({ gfx::Rect{ -2000.0f, GroundTop, 6000.0f, 120.0f }, Ground, gfx::Layer::Game, gfx::Space::World });

        for (int i = 0; i < 8; ++i) {
            const auto x = static_cast<float>(-800 + i * 320);
            const auto y = static_cast<float>(420 - (i % 4) * 90);
            scene.push_back({ gfx::Rect{ x, y, 180.0f, 28.0f }, Platform, gfx::Layer::Game, gfx::Space::World });
        }

        return scene;
    }

    void drawTopEdge(gfx::IRenderer& renderer, const gfx::RectDraw& object)
    {
        const float y = object.dest.y + EdgeThickness * 0.5f;

        renderer.drawLine({ gfx::Vec2{ object.dest.x, y }, gfx::Vec2{ object.dest.x + object.dest.w, y }, EdgeThickness,
                            gfx::lerp(object.color, gfx::White, EdgeLightness), object.layer, object.space });
    }

    void drawHudText(gfx::IRenderer& renderer, const gfx::Font& font, const playground::Mode mode)
    {
        const std::string text = std::string(mode == playground::Mode::Play ? "Play mode" : "Edit mode")
            + "  |  " + renderer.backendName();

        gfx::TextDraw draw;
        draw.font = &font;
        draw.text = text;
        draw.position = gfx::Vec2{ HudTextX, (HudHeight - font.lineHeight()) * 0.5f };
        draw.layer = gfx::Layer::Foreground;
        draw.space = gfx::Space::Screen;
        renderer.drawText(draw);
    }

    // CLion runs from the build directory: resolve next to the binary, where CMake copies assets/.
    std::string assetPath(const char* relative)
    {
        const char* base = SDL_GetBasePath();

        return std::string(base != nullptr ? base : "") + "assets/" + relative;
    }

    std::vector<uint8_t> readFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return {};
        }

        return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
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
    std::printf("Tab or HUD button toggles edit/play | edit: WASD/arrows pan | play: A/D/arrows walk, space hops\n");
    std::printf("edit: mouse wheel zooms | escape quits\n");
    std::printf("checks: R reloads the showcase texture mid-frame | F forces renderer misuse warnings (showcase + gallery)\n");

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

    // Without these sheets the bull still idles and hops on walk frames: not worth refusing to start over.
    const std::optional<tools::Image> idleSheet = tools::loadPng(assetPath("bongo_repos_spritesheet.png"));
    const gfx::TextureHandle idleTexture = idleSheet.has_value()
        ? renderer->createTexture(idleSheet->rgba.data(), idleSheet->width, idleSheet->height)
        : gfx::TextureHandle::Invalid;
    if (idleTexture == gfx::TextureHandle::Invalid) {
        std::printf("Could not load the bull idle sheet: it will stand still\n");
    }

    const std::optional<tools::Image> jumpSheet = tools::loadPng(assetPath("bongo_saut_spritesheet.png"));
    gfx::TextureHandle jumpTexture = jumpSheet.has_value()
        ? renderer->createTexture(jumpSheet->rgba.data(), jumpSheet->width, jumpSheet->height)
        : gfx::TextureHandle::Invalid;
    if (jumpTexture == gfx::TextureHandle::Invalid) {
        std::printf("Could not load the bull jump sheet: hops will use walk frames\n");
        jumpTexture = bullTexture;
    }

    // Created after the last early return above; it must die before the renderer.
    const std::vector<uint8_t> fontFile = readFile(assetPath("DejaVuSans.ttf"));
    std::unique_ptr<gfx::Font> font = gfx::Font::create(*renderer, fontFile, FontPixelHeight);
    if (font == nullptr) {
        std::printf("Could not create the font: the HUD will have no text\n");
    }

    playground::Player player(bullTexture, idleTexture, jumpTexture, gfx::Vec2{ 120.0f, GroundTop });
    playground::ModeToggle modeToggle(playground::Mode::Edit);
    playground::Gallery gallery(*renderer, bullTexture, font.get(), GalleryOrigin);

    // Holds a texture on the renderer: optional so it can be released before the renderer dies.
    std::optional<playground::Showcase> showcase;
    showcase.emplace(*renderer, *sheet, ShowcaseCenter);

    const std::vector<gfx::RectDraw> scene = makeScene();
    const playground::Scenery scenery(GroundTop);

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
                    } else if (event.key.scancode == SDL_SCANCODE_SPACE) {
                        if (modeToggle.mode() == playground::Mode::Play) {
                            player.jump();
                        }
                    } else if (event.key.scancode == SDL_SCANCODE_R) {
                        showcase->requestReload();
                    } else if (event.key.scancode == SDL_SCANCODE_F) {
                        showcase->misuse();
                        gallery.requestMisuse();
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

        scenery.drawBackground(*renderer, viewport);

        for (const gfx::RectDraw& object : scene) {
            renderer->drawRect(object);
            drawTopEdge(*renderer, object);
        }

        renderer->drawSprite(player.sprite());
        gallery.draw(*renderer);

        // Before the HUD: both are Foreground, so submission order keeps the bushes under it.
        scenery.drawForeground(*renderer);

        // Screen space ignores the camera: this bar stays put while the world scrolls under it.
        renderer->drawRect({ gfx::Rect{ 0.0f, 0.0f, viewport.x, HudHeight }, Hud, gfx::Layer::Foreground, gfx::Space::Screen });
        showcase->draw(viewport);
        modeToggle.draw(*renderer);
        if (font != nullptr) {
            drawHudText(*renderer, *font, modeToggle.mode());
        }

        renderer->endFrame();
    }

    showcase.reset();
    font.reset();
    if (idleTexture != gfx::TextureHandle::Invalid) {
        renderer->destroyTexture(idleTexture);
    }
    if (jumpTexture != bullTexture) {
        renderer->destroyTexture(jumpTexture);
    }
    renderer->destroyTexture(bullTexture);

    // The renderer holds an SDL_Renderer tied to this window: release it before the window dies.
    renderer.reset();

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
