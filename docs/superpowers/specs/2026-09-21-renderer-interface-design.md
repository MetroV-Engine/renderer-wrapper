# Renderer interface design

**Date:** 2026-09-21
**Workspace:** `renderer-wrapper`
**Status:** approved design, not yet implemented

## 1. Purpose

Give the engine one rendering interface whose backend can be swapped without touching ECS,
gameplay or editor code.

- **bgfx** is the target backend.
- **SDL3 CPU rendering** is the interim backend the UI team builds against.
- Switching between them must be a one-line change at the call site that creates the renderer.

The prototype (`MetroV-prototype`) shows exactly what this replaces: systems take the backend
directly, e.g. `DrawableSys::update(registry&, float dt, SDL_Window*, SDL_Renderer*)`, and draw
with `SDL_SetRenderDrawColor` + `SDL_RenderFillRect`. That signature is the leak being removed.

## 1.1 OPEN DECISION — revisit ASAP (2026-09-22)

**The viewport mechanism is provisional.** The spec below commits to a native child window
(`SDL_CreatePopupWindow`), but a simpler option exists and should be evaluated before the editor
phase (13.6) starts.

Both backends support rendering into a sub-rectangle of a window *natively*:
`bgfx::setViewRect(view, x, y, w, h)` and `SDL_SetRenderViewport(renderer, rect)`. That removes the
child window, the per-OS code and the popup-occlusion problem entirely.

The blocker is that **two graphics APIs cannot both own and present the same window**. If bgfx owns
the editor window, ImGui must be rendered *by bgfx*, which means adding to `IRenderer`:

```cpp
virtual void setViewport(const Rect& rect) = 0;
virtual void drawMesh(const Vertex* v, size_t nv, const uint16_t* i, size_t ni, TextureHandle) = 0;
virtual void setScissor(const Rect& rect) = 0;
```

ImGui draw data is textured triangles plus clip rects, so one backend-agnostic ImGui renderer
written on top of those three calls replaces every per-backend `imgui_impl_*`.

**Why it is deferred, not decided:** it changes nothing about the interface's drawing, camera,
layer or resource model, so phases 1-5 are unaffected either way. It only changes who owns the
editor window.

**Decide before:** phase 6. **Decide with:** a spike of each mechanism on X11 and Wayland.

Related SDL3 facts, verified in the 3.2 headers:

- `SDL_PROP_WINDOW_CREATE_PARENT_POINTER` is *"required for windows with the tooltip, menu, and
  modal properties"* — SDL3 parents popups only.
- `SDL_PROP_WINDOW_CREATE_X11_WINDOW_NUMBER` / `..._WIN32_HWND_POINTER` /
  `..._COCOA_VIEW_POINTER` wrap *an existing* native window. SDL adopts a child window; it never
  creates one.
- `SDL_SetRenderViewport` is `SDL_Renderer`-only and does nothing for a bgfx surface.

## 2. Scope

**In scope:** a backend-agnostic 2D drawing interface (rects and sprites), a 2D camera, layer
ordering, texture resources, and three implementations (bgfx, SDL3 CPU, null).

**Out of scope, deliberately:**

| Not included | Why | Where it belongs |
|---|---|---|
| Asset/file loading (PNG decode) | Keeps `stb_image`/`SDL_image` out of every backend and keeps backends symmetric | Engine workspace: decode to RGBA, call `createTexture` |
| Text rendering | Needs a font atlas, which is a texture + quads once that exists | Later, on top of this interface |
| ImGui bridge | The UI is an external editor window; ImGui never renders through bgfx | Editor workspace |
| Parallax | A per-layer camera factor, cheap to add because layers exist | Later |
| Batching API | Backends batch internally; callers must not have to think about it | Backend internals |
| Shaders in the interface | A 2D engine's callers should not author shaders | Backend internals |

## 3. The interface

```cpp
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Surface
    virtual void resize(uint32_t width, uint32_t height) = 0;

    // Frame
    virtual void beginFrame(Color clear) = 0;
    virtual void endFrame() = 0;

    // View
    virtual void setCamera(const Camera2D& camera) = 0;

    // Resources
    virtual TextureHandle createTexture(const uint8_t* rgba, uint32_t width, uint32_t height) = 0;
    virtual void destroyTexture(TextureHandle texture) = 0;
    virtual Vec2 textureSize(TextureHandle texture) const = 0;

    // Drawing
    virtual void drawRect(const RectDraw& draw) = 0;
    virtual void drawSprite(const SpriteDraw& draw) = 0;

    // Diagnostics
    virtual const char* backendName() const = 0;
};
```

Eleven virtuals including the destructor. There is no `initialize()` and no `shutdown()`: a
renderer that exists is initialized, the factory returns `nullptr` on failure, and the destructor
tears down. Two-phase initialisation would put an `if (!initialize())` dance at every call site
and allow half-constructed renderers to exist.

### 3.1 Why draw calls take structs

```cpp
struct SpriteDraw {
    TextureHandle texture;
    Rect source;                     // pixels within the texture; w or h <= 0 means the whole texture
    Rect dest;                       // world units, or screen pixels when space == Screen
    float rotation = 0.0f;           // radians, clockwise (y-down), about the centre of dest
    Layer layer = 0;
    Space space = Space::World;
    Color tint = Color::White;
};

struct RectDraw {
    Rect dest;
    Color color;
    Layer layer = 0;
    Space space = Space::World;
};
```

Adding `flipX`, `origin` or `outline` later becomes a new field with a default, not a signature
change rippling through both backends and every call site. It also removes the need for separate
`drawSpriteScreen` / `drawRectScreen` entry points: screen space is a field.

## 4. Types

```cpp
struct Vec2  { float x = 0.0f, y = 0.0f; };
struct Rect  { float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f; };
struct Color { uint8_t r = 0, g = 0, b = 0, a = 255; static const Color White; };
struct Camera2D { Vec2 center; float zoom = 1.0f; };

enum class TextureHandle : uint32_t { Invalid = 0 };
enum class Space : uint8_t { World, Screen };
using Layer = int;
```

`IRenderer.hpp` includes `<cstdint>` and nothing else. No SDL, no bgfx, no engine headers.

### 4.1 Coordinate system, fixed

- **World units are pixels at `zoom == 1.0`.** Existing `Position`/`Bounds` values and `scene.json`
  keep their meaning.
- **Y grows downward**, matching SDL, the prototype's data and editor mouse coordinates. For bgfx
  this is one flipped orthographic matrix. Choosing y-up would silently invert every saved scene.
- `Camera2D::center` is the world point at the centre of the viewport. `zoom > 1` magnifies.
- **The default camera is `center = {width / 2, height / 2}, zoom = 1`**, set at construction from
  the initial surface size. World coordinates are then identical to the prototype's screen pixels,
  which is what makes the ECS migration in section 12 a no-op for existing values. `resize()` does
  not change the camera; a game that wants the view re-centred sets it itself.
- `Space::Screen` coordinates are pixels from the top-left of the render surface and ignore the
  camera entirely. This is what `ScreenLocked` entities and HUD use.

### 4.2 Shared transform, not per-backend maths

```cpp
Vec2 worldToScreen(const Camera2D&, Vec2 world, Vec2 viewportSize);
```

A free function in a shared header, used by every backend and unit-tested on its own. Two backends
deriving the same transform independently is how "it's 3 pixels off in bgfx" bugs are born.

## 5. Window ownership

The renderer never creates, owns, resizes or destroys a window.

```cpp
enum class NativeWindowKind : uint8_t { X11, Wayland, Win32, Cocoa };
struct NativeWindow { void* nwh = nullptr; void* ndt = nullptr; NativeWindowKind kind; };

NativeWindow nativeWindowFromSdl(SDL_Window*);   // SdlWindowBridge.hpp, app/editor layer only
```

> **Provisional — see section 1.1.** A sub-rect viewport in a single window may replace this.

This is what makes the editor plan work. The editor creates a **native child window** for the
viewport with `SDL_CreatePopupWindow(parent, offsetX, offsetY, w, h, SDL_WINDOW_POPUP_MENU |
SDL_WINDOW_NOT_FOCUSABLE)`, converts it, and hands it over. bgfx renders directly into that child
window — no GPU-to-CPU readback, no texture blit. When the viewport rect changes, the editor moves
the child window and calls `resize()`.

### 5.1 Factory

```cpp
enum class Backend : uint8_t { Bgfx, Sdl3Cpu, Null };

struct SDL_Window;   // forward declaration: no SDL header in this interface

std::unique_ptr<IRenderer> makeRenderer(Backend backend, SDL_Window* window,
                                        uint32_t width, uint32_t height);
```

The factory takes the `SDL_Window*` rather than a `NativeWindow`, because `SDL_CreateRenderer`
needs the window object itself and cannot work from raw native handles. `SDL_Window` is the
windowing type both backends share, not a rendering-backend type, and the forward declaration keeps
SDL out of the header. `NativeWindow` and `nativeWindowFromSdl` remain, used by the bgfx backend
only.

Returns `nullptr` on failure, after logging the reason. `Backend::Null` ignores the window
argument.

## 6. Frame flow

```
beginFrame(clear) -> setCamera(cam) -> drawRect/drawSprite (xN) -> endFrame()
```

- `beginFrame(clear)` clears the entire surface to `clear`, both spaces, every layer.
- `setCamera` may be called at any point; it affects subsequent `Space::World` draws only.
- `endFrame()` presents. Nothing is guaranteed visible before it returns.
- Draw calls outside a begin/end pair are ignored and logged once.

### 6.1 Layers

Layers draw in ascending order; within one layer, submission order is preserved. ECS iteration
order is component-storage order and carries no meaning, so depth must be explicit.

- Valid range: `0..63`. Out-of-range values are clamped, logged once.
- **bgfx:** layer `n` maps to view id `n + 1`; view 0 is reserved for the frame clear. bgfx sorts
  submissions per view internally.
- **SDL3 CPU:** one bucket per layer, flushed in ascending order inside `endFrame()`.

## 7. Resources

- `createTexture` takes tightly packed, non-premultiplied **RGBA8** pixels and **copies them**. The
  caller may free its buffer immediately on return.
- Standard alpha blending is always enabled. There is no blend-mode knob until something needs one.
- `textureSize` on an invalid handle returns `{0, 0}`.
- `destroyTexture` called between `beginFrame` and `endFrame` is **deferred until after
  `endFrame()`**, because draws are queued and both backends read texture memory later in the
  frame. Destroying immediately would be the same dangling-pointer class of bug as `bgfx::makeRef`
  over a local buffer.
- Handles are opaque; each backend maps them to its own resources through its own table.

## 8. Error handling

No exceptions in the render loop, and no crash on bad input from the editor:

| Situation | Behaviour |
|---|---|
| Backend fails to initialise | `makeRenderer` returns `nullptr`, reason logged |
| Draw with an invalid/destroyed `TextureHandle` | Skipped, logged once per handle |
| Layer out of range | Clamped to `0..63`, logged once |
| `resize(0, h)` or `resize(w, 0)` | Ignored |
| Double `destroyTexture` | No-op, logged once |

Rationale: the editor reloads scenes and will hand the renderer stale handles. That must never take
the application down.

## 9. Threading

Every `IRenderer` call happens on the thread that created it. bgfx is initialised in
single-threaded mode (`bgfx::renderFrame()` before `bgfx::init()`), so this is a hard requirement,
not a convention.

## 10. Backends

| Backend | Purpose | Notes |
|---|---|---|
| `BgfxRenderer` | Target backend | Extracted from today's `Renderer`; one textured-quad program, layers as views |
| `SdlRenderer` | UI team's interim backend | `SDL_Renderer` + `SDL_Texture`; camera applied through the shared `worldToScreen`, never through `SDL_SetRenderScale`, so both backends land pixels identically |
| `NullRenderer` | Tests and CI | Records draw calls, renders nothing, needs no window |

`NullRenderer` is roughly 40 lines and is what makes the workspace testable: ECS systems can be
asserted against recorded draw calls with no display, so CI can run them.

## 11. File layout

Project convention: **one unit, one directory** for anything with a `.cpp`, named after the class
it holds. Header-only units stay at the group root.

```
Renderer/
  RendererTypes.hpp              Vec2, Rect, Color, Camera2D, handles, Space, Layer
  IRenderer.hpp                  the interface; includes <cstdint> and RendererTypes.hpp only
  Transform/
    Transform.hpp/.cpp           worldToScreen and friends, unit-tested
  RendererFactory/
    RendererFactory.hpp/.cpp     makeRenderer
  SdlWindowBridge/
    SdlWindowBridge.hpp/.cpp     nativeWindowFromSdl, for the bgfx backend
  SdlRenderer/
    SdlRenderer.hpp/.cpp         SDL3 backend
  NullRenderer/
    NullRenderer.hpp/.cpp        records draws, needs no window
  Renderer/
    Renderer.hpp/.cpp            today's bgfx triangle; becomes BgfxRenderer/ in phase 1
```

## 12. ECS migration

```diff
- void DrawableSys::update(registry&, float dt, SDL_Window*, SDL_Renderer*);
+ void DrawableSys::update(registry&, float dt, IRenderer&);
```

`Position` + `Bounds` become a `RectDraw` with no value changes, because world units are y-down
pixels. `ScreenLocked` entities set `space = Space::Screen`.

## 13. Phases

1. Extract `IRenderer` + `RendererTypes` + factory; today's `Renderer` becomes `BgfxRenderer`. The
   triangle still renders, now through the interface.
2. `NullRenderer` + unit tests for `worldToScreen` and for recorded draw calls.
3. Textures and `drawSprite` on bgfx: textured-quad program, handle table, layers as views.
4. `SdlRenderer` (SDL3 CPU) implementing the same interface, validated against the same tests.
5. ECS migration: `DrawableSys` and friends take `IRenderer&`.
6. Editor integration: child viewport window, `resize()` on rect change.

## 14. Risks

| Risk | Mitigation |
|---|---|
| SDL3 popup windows are meant for transient menus; using one as a persistent viewport is off the beaten path | Spike it on X11 **and** Wayland before phase 6 commits; the sub-rect alternative in section 1.1 avoids the question entirely |
| ImGui popups overlapping the viewport are occluded by the native child window | Editor enables `ImGuiConfigFlags_ViewportsEnable` so popups become real OS windows |
| bgfx caps view count (256 by default) | 64 layers map to views 1..64, leaving ample headroom |
| The two backends' transforms drift apart | Single shared `worldToScreen`, unit-tested once |
