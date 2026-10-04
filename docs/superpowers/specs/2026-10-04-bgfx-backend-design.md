# bgfx backend design

**Date:** 2026-10-04
**Workspace:** `renderer-wrapper`
**Status:** design awaiting review
**Builds on:** `2026-09-21-renderer-interface-design.md` (phases 1 and 3). Where the two disagree,
this document wins; section 10 lists the edits to apply to the older spec.

## 1. Goal

Make bgfx a real `IRenderer` backend, GPU-rendered, and the playground's default.

**Done means:** the playground (walking bull, edit/play toggle, Showcase: rotation, tint,
screen-space whole-sheet strip, R mid-frame reload, F misuse warnings) looks and behaves the same
on bgfx as on SDL, with `--backend=sdl` still available.

**Constraint from the start:** the design must not need undoing when lights, shadows and other
GPU effects arrive. Section 9 shows the path; nothing in it is built now.

## 2. Approach: batched quads, camera on the GPU

Every draw becomes one textured quad. Quads are built on the CPU in **draw space** (world units
for `Space::World`, pixels for `Space::Screen`), batched into transient buffers, and transformed
by the GPU:

- world batches: `model = cameraMatrix(camera, viewport)`;
- screen batches: `model = identity`;
- every batch: `proj = pixel ortho` (0..w, 0..h, y down), `view = identity`.

All GPU work — rasterisation, texture sampling, blending, shading — is on the GPU. The CPU only
writes 4 vertices per draw, which is how 2D engines batch (MonoGame `SpriteBatch`, libGDX, Godot 2D).

Rejected:
- **Layers as views** (old spec §6.1): a second camera implementation per view, world/screen
  interleaving split across views, per-draw model matrices for rotation.
- **Instancing:** throughput a 2D game does not need yet, transform logic duplicated into the
  shader. Batching is behind the interface, so it can move to instancing without caller changes.

## 3. Units

```
                      IRenderer (unchanged)
          ┌───────────────┼────────────────┐
     SdlRenderer     BgfxRenderer      NullRenderer
          │               │
          │      ┌────────┴──────────┐
          │      │  renderer_core (no SDL / bgfx, unit-tested)
          ├──────┤   Transform      worldToScreen          (exists)
          │      │   QuadBuilder    draw -> 4 vertices     (new)
          │      │   CameraMatrix   camera -> 4x4 matrix   (new)
          │      └───────────────────┘
          │               │ uses
          │      SdlWindowBridge  (exists; its copy inside Renderer.cpp is deleted)
          │      BgfxProgram      embedded sprite shaders for the active API (new)
```

| Unit | Location | Responsibility |
|---|---|---|
| `QuadBuilder` | `Renderer/QuadBuilder/` (core) | `SpriteDraw`/`RectDraw` + texture size -> 4 `QuadVertex` in draw space |
| `CameraMatrix` | `Renderer/CameraMatrix/` (core) | `Camera2D` + viewport -> column-major `float[16]`, world -> pixels |
| `BgfxProgram` | `Renderer/BgfxProgram/` | Load embedded `vs_sprite`/`fs_sprite` for OpenGL or Vulkan; RAII program |
| `BgfxRenderer` | `Renderer/BgfxRenderer/` | `IRenderer`: bgfx lifetime, texture table, layer queues, batching, deferred destroy |

Removed: `Renderer/Renderer/` (triangle), `shaders/vs_simple.sc`, `shaders/fs_simple.sc`.

### 3.1 QuadBuilder

```cpp
struct QuadVertex { float x, y, u, v; uint8_t r, g, b, a; };   // 20 bytes, matches the bgfx layout
using Quad = std::array<QuadVertex, 4>;                         // TL, TR, BR, BL

Quad buildQuad(const SpriteDraw& draw, Vec2 textureSize);
Quad buildQuad(const RectDraw& draw);                           // uv 0..1 over the white texture
```

- Corners come from `dest`, rotated by `rotation` radians **clockwise in y-down space** about the
  centre of `dest`: `x' = cx + dx·cos − dy·sin`, `y' = cy + dx·sin + dy·cos` (same sense as
  `SDL_RenderTextureRotated`).
- UV = `source / textureSize`; `source.w <= 0 || source.h <= 0` -> whole texture `0..1`.
- `flipX` swaps `u0`/`u1`. `tint` (or rect `color`) is copied to every vertex.
- `textureSize` with a zero component -> uv `0..1`, never a division by zero.

### 3.2 CameraMatrix

```cpp
std::array<float, 16> cameraMatrix(const Camera2D& camera, Vec2 viewport);   // column-major
```

`x' = (x − cx)·z + w/2`, `y' = (y − cy)·z + h/2`, z unchanged; zoom `<= 0` treated as 1, exactly
like `Transform`'s `safeZoom`. A unit test asserts it equals `worldToScreen` for a grid of
cameras and points: the drift guard between the two backends.

## 4. Frame flow

```
beginFrame(clear) -> setCamera -> drawRect / drawSprite (xN) -> endFrame()
```

- **`beginFrame`:** remember `clear`, empty the 64 layer queues, enter the frame.
- **draws:** validated and queued exactly like `SdlRenderer` (same warn-once rules, same layer
  clamp). Nothing touches bgfx before `endFrame`.
- **`endFrame`:**
  1. View 0: `setViewClear(colour)`, `setViewRect(0, 0, w, h)`,
     `setViewMode(ViewMode::Sequential)`, `setViewTransform(identity, pixelOrtho)`, `touch`.
     `pixelOrtho = bx::mtxOrtho(0, w, h, 0, 0, 1, 0, caps->homogeneousDepth)` (bottom = h, top = 0
     gives y down).
  2. Layers ascending; inside a layer, submission order. Consecutive draws with the same
     **(texture, space)** form one batch.
  3. Per batch: `allocTransientBuffers` (16-bit indices, so at most 16 384 quads per submit; larger
     runs split), fill with `buildQuad`, `setTransform(world ? camera : identity)`, set buffers,
     `setTexture(0, s_texColor, texture)`, `setState(WRITE_RGB | WRITE_A | BLEND_ALPHA)`,
     `submit(0, program)`. No depth test or write: order is submission order.
  4. `bgfx::frame()`, leave the frame, release textures whose destroy was deferred.
- **`resize`:** ignore zero; otherwise `bgfx::reset(VSYNC)` with the new size (as today's
  `Renderer::resize`). The camera is not moved.

## 5. Resources

- **Vertex layout:** `Position` 2×float, `TexCoord0` 2×float, `Color0` 4×uint8 normalised.
- **`createTexture`:** reject null/zero (as SDL) and sizes above `caps->limits.maxTextureSize`
  (log, `Invalid`). `createTexture2D(w, h, false, 1, RGBA8, U/V clamp, bgfx::copy(rgba, w·h·4))`;
  the caller may free its pixels on return. Table: id -> `{bgfx handle, size}`.
- **Filtering:** linear, matching SDL3's default scale mode so both backends look the same. A
  per-texture filter (pixel art) is a later `createTexture` option, not part of this work.
- **`destroyTexture`:** inside a frame -> deferred until after `bgfx::frame()` (draws are only
  submitted in `endFrame`); outside -> immediate; unknown handle -> warn once.
- **White texture:** 1×1 RGBA `FFFFFFFF`, created at init, used by every rect batch, not in the
  handle table.
- **Shutdown order (destructor):** user textures, white texture, uniform, program, then
  `bgfx::shutdown()`.
- **One instance:** bgfx is process-global; `create()` refuses a second live `BgfxRenderer`.

## 6. Shaders and build

```
varying.def.sc
  vec2 a_position  : POSITION;
  vec2 a_texcoord0 : TEXCOORD0;
  vec4 a_color0    : COLOR0;
  vec2 v_texcoord0 : TEXCOORD0;
  vec4 v_color0    : COLOR0;

vs_sprite.sc   gl_Position = mul(u_modelViewProj, vec4(a_position, 0.0, 1.0)); pass uv, colour
fs_sprite.sc   SAMPLER2D(s_texColor, 0); gl_FragColor = texture2D(s_texColor, v_texcoord0) * v_color0;
```

CMake compiles `vs_sprite`/`fs_sprite` instead of the simple pair, same profiles (`430`, `spirv`),
same embedded-header mechanism. `QuadBuilder` and `CameraMatrix` join `renderer_core`;
`BgfxProgram` and `BgfxRenderer` join `renderer_wrapper`.

## 7. Playground and backend selection

- `main.cpp` parses `--backend=bgfx` (default) or `--backend=sdl`; anything else prints usage
  and exits 1. Everything after renderer creation is unchanged: Player, ModeToggle and Showcase
  only know `IRenderer`.
- A second CLion run configuration, `renderer_wrapper (SDL)`, passes `--backend=sdl`.
- The x11 video-driver hint stays.

## 8. Error handling

| Situation | Behaviour |
|---|---|
| Native window handles unavailable, `bgfx::init` fails, no embedded shader for the chosen API | `create()` logs, returns `nullptr`; `main` exits 1 |
| Second `BgfxRenderer` while one is alive | `nullptr`, logged |
| Draw outside a frame, layer out of range, invalid texture, double `beginFrame`/`endFrame` | Same as `SdlRenderer`: ignored or clamped, warned once (prefix `[BgfxRenderer]`) |
| Texture larger than the GPU limit | `Invalid`, logged |
| Transient buffers exhausted mid-frame | Remaining quads that frame are dropped, warned once |

## 9. Path to lights and effects (not built now)

Each step adds to this design; none replaces it.

1. **Offscreen scene:** view 0 renders into a framebuffer instead of the back-buffer
   (`setViewFrameBuffer`). The blend state must then become separate-alpha
   (`BGFX_STATE_BLEND_FUNC_SEPARATE(SRC_ALPHA, INV_SRC_ALPHA, ONE, INV_SRC_ALPHA)`) so the offscreen alpha channel stays meaningful.
2. **Light pass:** a new view draws lights additively into a light-map framebuffer. Vertex
   positions are already world positions before `u_model`, so the shader passes them on as
   `v_worldPos`. Interface addition: `drawLight(const LightDraw&)`.
3. **Composite:** a final view multiplies scene by light-map onto the back-buffer.
   `Space::Screen` draws (HUD) then render **after** the composite so they stay unlit. This is a
   deliberate future change: today screen and world draws interleave by layer; with lighting,
   screen-space always lands on top. Nothing current relies on HUD under world.
4. **Normal maps:** `SpriteDraw` gains `TextureHandle normalMap = Invalid`; the quad already
   carries the UVs a second sampler needs.
5. **Materials:** "one program" becomes "one program per material"; batches already break on
   texture changes and would also break on material changes.
6. **Shadows:** occluder geometry rendered per light into shadow maps, another set of views.

## 10. Edits to the 2026-09-21 spec

- §6.1 Layers, bgfx bullet: replace "layer n -> view n+1" with "one sequential view per pass;
  layers ordered by submission (see bgfx backend design)".
- §10 Backends, `BgfxRenderer` row: "batched textured quads, camera as GPU matrix".
- §11 File layout: add `QuadBuilder/`, `CameraMatrix/`, `BgfxProgram/`, `BgfxRenderer/`; drop
  `Renderer/`.
- §13 Phases: mark 1-4 done once this lands.
- §14 Risks: drop "bgfx caps view count" (no longer applies).

## 11. Testing

- **Unit (`renderer_tests`, headless):**
  - `QuadBuilderTests`: axis-aligned corners; source rect -> uv; whole texture -> `0..1`;
    `flipX` swaps u; tint on all corners; rotation π/2 moves the top-left corner to the top-right;
    rect uses uv `0..1` and its colour; zero texture size does not divide by zero.
  - `CameraMatrixTests`: equals `worldToScreen` over several cameras and points; zoom `<= 0` acts
    as 1; default camera is identity in pixel space.
- **Manual (playground, both backends, side by side):** the existing checklist — walk, flip,
  idle, edit/play toggle, wheel only in edit, Showcase rotation/tint/strip, R without
  flicker or warnings, F's five warnings, resize — plus the log line naming bgfx's API
  (OpenGL or Vulkan).
- Not covered: `BgfxRenderer` itself headless (needs a window and GPU).

## 12. Out of scope

Lights and effects (section 9 only), per-texture filtering, the editor viewport decision (old
spec §1.1, before phase 6), Wayland as default, Windows/macOS verification, instancing.

## 13. Risks

| Risk | Mitigation |
|---|---|
| Linear filtering samples neighbouring frames at sheet edges (bleeding) | Current sheets have transparent margins; if it shows, inset UVs by half a texel or pad sheets |
| bgfx picks Vulkan on one machine, OpenGL on another | Both shader profiles are embedded; the log names the API |
| y-down orthographic matrix wrong (image upside down) | `CameraMatrix` test pins the maths; the ortho is one line, checked visually on first run |
| bgfx is a global | Single-instance guard in `create()` |
