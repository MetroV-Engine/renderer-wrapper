# bgfx Backend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make bgfx a GPU-rendered `IRenderer` backend and the playground's default, visually identical to the SDL backend.

**Architecture:** Draws are queued per layer like `SdlRenderer`. At `endFrame` they become textured quads (pure `QuadBuilder`, draw-space vertices), batched by (texture, space) into bgfx transient buffers on one sequential view. The camera is a GPU model matrix (pure `CameraMatrix`, unit-tested against `worldToScreen`); screen-space batches use identity.

**Tech Stack:** C++20, CMake ≥ 3.24, bgfx.cmake `v1.161.9495-578` (FetchContent), SDL 3.4, GoogleTest v1.18.0, CLion.

**Spec:** `docs/superpowers/specs/2026-10-04-bgfx-backend-design.md` (builds on `2026-09-21-renderer-interface-design.md`).

## Global Constraints

- C++20, `-Wall -Wextra -Wpedantic` (MSVC `/W4`) must stay clean.
- No backend type (`bgfx::`, `SDL_Renderer*`) in `IRenderer.hpp` / `RendererTypes.hpp`; `renderer_core` links neither SDL nor bgfx.
- One unit, one directory for anything with a `.cpp`; tests flat in `Tests/<Unit>Tests.cpp`.
- Every new file starts with the project banner (copy lines 1-12 of `Renderer/IRenderer.hpp`, set `Created on: 10/4/26`, `Filename`, `Description`).
- **The human builds and commits.** Agents never build/compile and never run git write commands. "Build" steps are done by the human in CLion, who pastes the output. Agents may run an already-built `cmake-build-debug/renderer_tests` binary.
- **File creation exception (2026-10-04, this plan only):** the agent creates the new files and folders itself instead of asking for CLion's *New > C++ Class*.
- Comments: one line, only the non-obvious *why*.
- bgfx view 0 is the scene view, `ViewMode::Sequential`; vertex layout is Position 2×float, TexCoord0 2×float, Color0 4×uint8 normalised (20 bytes).
- World y points down; `worldToScreen` and `cameraMatrix` must agree; zoom `<= 0` is treated as 1 everywhere.
- Linear filtering, U/V clamp, non-premultiplied alpha blending (`BGFX_STATE_BLEND_ALPHA`).
- Warnings mirror `SdlRenderer` wording with the `[BgfxRenderer]` prefix.

## Review Focus

1. **Window resize on bgfx:** the back-buffer and pixel ortho follow the new size; the HUD bar stretches and the mode button stays clickable (Task 4, manual step).
2. **Minimise / zero-size window:** `resize(0, h)` is ignored, no `bgfx::reset` with zero, restore draws normally (Task 4, manual step).
3. **R (mid-frame destroy + recreate) on bgfx:** no flicker, no warning, no crash, handle increments (Task 4, manual step).
4. **Rotation combined with `flipX`:** positions rotate, UVs stay mirrored (Task 2, `FlipAndRotationCombine`).
5. **Unknown command-line argument:** prints usage, exits 1, never opens a half-initialised window (Task 4, manual step).

---

## File Map

| File | Action | Responsibility |
|---|---|---|
| `Renderer/CameraMatrix/CameraMatrix.{hpp,cpp}` | create | `Mat4 cameraMatrix(Camera2D, Vec2)`, `Mat4 identityMatrix()` |
| `Renderer/QuadBuilder/QuadBuilder.{hpp,cpp}` | create | `QuadVertex`, `Quad`, `QuadIndices`, `buildQuad(...)` |
| `Renderer/BgfxProgram/BgfxProgram.{hpp,cpp}` | create | RAII sprite program from embedded GLSL/SPIR-V |
| `Renderer/BgfxRenderer/BgfxRenderer.{hpp,cpp}` | create | the bgfx `IRenderer` |
| `shaders/vs_sprite.sc`, `shaders/fs_sprite.sc` | create | sprite shaders |
| `shaders/varying.def.sc` | modify | position/uv/colour varyings |
| `shaders/vs_simple.sc`, `shaders/fs_simple.sc`, `Renderer/Renderer/` | delete | triangle |
| `Tests/CameraMatrixTests.cpp`, `Tests/QuadBuilderTests.cpp` | create | unit tests |
| `Renderer/RendererFactory/RendererFactory.cpp` | modify | `Backend::Bgfx` -> `BgfxRenderer::create` |
| `main.cpp` | modify | `--backend=bgfx|sdl`, default bgfx |
| `CMakeLists.txt` | modify | sources, shaders, tests |
| `CLAUDE.md`, `docs/superpowers/specs/2026-09-21-renderer-interface-design.md` | modify | layout and spec edits |

---

### Task 1: CameraMatrix

**Files:**
- Create: `Renderer/CameraMatrix/CameraMatrix.hpp`, `Renderer/CameraMatrix/CameraMatrix.cpp`
- Create: `Tests/CameraMatrixTests.cpp`
- Modify: `CMakeLists.txt` (`renderer_core` sources, `renderer_tests` sources)

**Interfaces:**
- Consumes: `gfx::Camera2D`, `gfx::Vec2` (`Renderer/RendererTypes.hpp`); `gfx::worldToScreen` (`Renderer/Transform/Transform.hpp`, tests only).
- Produces: `using gfx::Mat4 = std::array<float, 16>;` `gfx::Mat4 gfx::cameraMatrix(const Camera2D& camera, Vec2 viewportSize);` `gfx::Mat4 gfx::identityMatrix();` — column-major, translation in `[12]`, `[13]`.

- [ ] **Step 1: Create the header (declarations only) and register files in CMake**

`Renderer/CameraMatrix/CameraMatrix.hpp` (after the banner, description `Camera as a GPU matrix: the twin of worldToScreen.`):

```cpp
#ifndef RENDERER_WRAPPER_CAMERAMATRIX_HPP
#define RENDERER_WRAPPER_CAMERAMATRIX_HPP

#include "Renderer/RendererTypes.hpp"

#include <array>

namespace gfx {

    // Column-major, the layout bgfx::setTransform reads.
    using Mat4 = std::array<float, 16>;

    // World units -> back-buffer pixels; must agree with worldToScreen for the same inputs.
    Mat4 cameraMatrix(const Camera2D& camera, Vec2 viewportSize);

    Mat4 identityMatrix();

}

#endif //RENDERER_WRAPPER_CAMERAMATRIX_HPP
```

`Renderer/CameraMatrix/CameraMatrix.cpp` (banner, same description), empty body for now:

```cpp
#include "Renderer/CameraMatrix/CameraMatrix.hpp"
```

`CMakeLists.txt`, in `add_library(renderer_core STATIC ...)` after the SpriteAnimation lines:

```cmake
        Renderer/CameraMatrix/CameraMatrix.cpp
        Renderer/CameraMatrix/CameraMatrix.hpp
```

and in `add_executable(renderer_tests ...)` after `Tests/NullRendererTests.cpp`:

```cmake
        Tests/CameraMatrixTests.cpp
```

- [ ] **Step 2: Write the failing tests**

`Tests/CameraMatrixTests.cpp` (banner, description `GPU camera matrix tests: must match worldToScreen.`):

```cpp
#include "Renderer/CameraMatrix/CameraMatrix.hpp"
#include "Renderer/Transform/Transform.hpp"

#include <gtest/gtest.h>

namespace {

    constexpr gfx::Vec2 Viewport{ 1280.0f, 720.0f };

    gfx::Camera2D makeCamera(const float x, const float y, const float zoom)
    {
        gfx::Camera2D camera;
        camera.center = gfx::Vec2{ x, y };
        camera.zoom = zoom;

        return camera;
    }

    // Column-major: x' = m0*x + m4*y + m12, y' = m1*x + m5*y + m13.
    gfx::Vec2 apply(const gfx::Mat4& m, const gfx::Vec2 p)
    {
        return gfx::Vec2{ m[0] * p.x + m[4] * p.y + m[12], m[1] * p.x + m[5] * p.y + m[13] };
    }

}

TEST(CameraMatrix, MatchesWorldToScreen)
{
    const gfx::Camera2D cameras[] = {
        makeCamera(640.0f, 360.0f, 1.0f),
        makeCamera(0.0f, 0.0f, 2.0f),
        makeCamera(-750.0f, 1200.0f, 0.2f),
        makeCamera(42.0f, -17.0f, 8.0f),
    };
    const gfx::Vec2 points[] = { { 0.0f, 0.0f }, { 123.5f, -987.25f }, { -4000.0f, 600.0f }, { 3000.0f, 3000.0f } };

    for (const gfx::Camera2D& camera : cameras) {
        const gfx::Mat4 matrix = gfx::cameraMatrix(camera, Viewport);

        for (const gfx::Vec2& world : points) {
            const gfx::Vec2 expected = gfx::worldToScreen(camera, world, Viewport);
            const gfx::Vec2 actual = apply(matrix, world);

            EXPECT_NEAR(actual.x, expected.x, 1e-2f);
            EXPECT_NEAR(actual.y, expected.y, 1e-2f);
        }
    }
}

TEST(CameraMatrix, NonPositiveZoomActsAsOne)
{
    const gfx::Mat4 unit = gfx::cameraMatrix(makeCamera(10.0f, 20.0f, 1.0f), Viewport);

    for (const float zoom : { 0.0f, -3.0f }) {
        EXPECT_EQ(gfx::cameraMatrix(makeCamera(10.0f, 20.0f, zoom), Viewport), unit);
    }
}

TEST(CameraMatrix, DefaultCameraIsIdentity)
{
    EXPECT_EQ(gfx::cameraMatrix(makeCamera(640.0f, 360.0f, 1.0f), Viewport), gfx::identityMatrix());
}

TEST(CameraMatrix, IsColumnMajor2DAffine)
{
    const gfx::Mat4 m = gfx::cameraMatrix(makeCamera(100.0f, 50.0f, 2.0f), Viewport);

    EXPECT_FLOAT_EQ(m[0], 2.0f);
    EXPECT_FLOAT_EQ(m[5], 2.0f);
    EXPECT_FLOAT_EQ(m[10], 1.0f);
    EXPECT_FLOAT_EQ(m[15], 1.0f);
    EXPECT_FLOAT_EQ(m[12], 640.0f - 200.0f);
    EXPECT_FLOAT_EQ(m[13], 360.0f - 100.0f);
    EXPECT_FLOAT_EQ(m[3], 0.0f);
    EXPECT_FLOAT_EQ(m[7], 0.0f);
}

TEST(CameraMatrix, IdentityIsIdentity)
{
    const gfx::Mat4 m = gfx::identityMatrix();

    for (size_t i = 0; i < 16; ++i) {
        EXPECT_FLOAT_EQ(m[i], (i % 5 == 0) ? 1.0f : 0.0f);
    }
}
```

- [ ] **Step 3: Human builds `renderer_tests` — expect failure**

Human: CLion ▸ Reload CMake Project, then Build ▸ Build 'renderer_tests'; paste output.
Expected: link error `undefined reference to gfx::cameraMatrix(...)` and `gfx::identityMatrix()`.

- [ ] **Step 4: Implement**

`Renderer/CameraMatrix/CameraMatrix.cpp`, replace the body:

```cpp
#include "Renderer/CameraMatrix/CameraMatrix.hpp"

namespace gfx {

    Mat4 identityMatrix()
    {
        Mat4 m{};
        m[0] = 1.0f;
        m[5] = 1.0f;
        m[10] = 1.0f;
        m[15] = 1.0f;

        return m;
    }

    Mat4 cameraMatrix(const Camera2D& camera, const Vec2 viewportSize)
    {
        // Same rule as Transform's safeZoom, or the two backends disagree on a zero zoom.
        const float zoom = camera.zoom > 0.0f ? camera.zoom : 1.0f;

        Mat4 m = identityMatrix();
        m[0] = zoom;
        m[5] = zoom;
        m[12] = viewportSize.x * 0.5f - camera.center.x * zoom;
        m[13] = viewportSize.y * 0.5f - camera.center.y * zoom;

        return m;
    }

}
```

- [ ] **Step 5: Human builds; agent runs the tests**

Human: Build 'renderer_tests'; paste output (no warnings).
Agent: `cmake-build-debug/renderer_tests --gtest_filter='CameraMatrix.*'`
Expected: `[  PASSED  ] 5 tests.` Then `cmake-build-debug/renderer_tests --gtest_brief=1` → `[  PASSED  ] 29 tests.`

- [ ] **Step 6: Hand off for commit**

Human reviews and commits. Suggested message: `feat(renderer): CameraMatrix, the GPU twin of worldToScreen`.

---

### Task 2: QuadBuilder

**Files:**
- Create: `Renderer/QuadBuilder/QuadBuilder.hpp`, `Renderer/QuadBuilder/QuadBuilder.cpp`
- Create: `Tests/QuadBuilderTests.cpp`
- Modify: `CMakeLists.txt` (`renderer_core`, `renderer_tests`)

**Interfaces:**
- Consumes: `gfx::SpriteDraw`, `gfx::RectDraw`, `gfx::Rect`, `gfx::Vec2`, `gfx::Color` (`Renderer/RendererTypes.hpp`).
- Produces:
  ```cpp
  struct gfx::QuadVertex { float x, y, u, v; uint8_t r, g, b, a; };   // sizeof == 20
  using gfx::Quad = std::array<QuadVertex, 4>;                         // TL, TR, BR, BL before rotation
  inline constexpr std::array<uint16_t, 6> gfx::QuadIndices{ 0, 1, 2, 0, 2, 3 };
  gfx::Quad gfx::buildQuad(const SpriteDraw& draw, Vec2 textureSize);
  gfx::Quad gfx::buildQuad(const RectDraw& draw);
  ```

- [ ] **Step 1: Create the header and an empty .cpp; register in CMake**

`Renderer/QuadBuilder/QuadBuilder.hpp` (banner, description `Turns a draw into four GPU-ready vertices: backend-free.`):

```cpp
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
```

`Renderer/QuadBuilder/QuadBuilder.cpp` (banner):

```cpp
#include "Renderer/QuadBuilder/QuadBuilder.hpp"
```

`CMakeLists.txt`: add to `renderer_core` after the CameraMatrix lines:

```cmake
        Renderer/QuadBuilder/QuadBuilder.cpp
        Renderer/QuadBuilder/QuadBuilder.hpp
```

and to `renderer_tests` after `Tests/CameraMatrixTests.cpp`:

```cmake
        Tests/QuadBuilderTests.cpp
```

- [ ] **Step 2: Write the failing tests**

`Tests/QuadBuilderTests.cpp` (banner, description `Quad geometry, UV and colour tests for QuadBuilder.`):

```cpp
#include "Renderer/QuadBuilder/QuadBuilder.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>

namespace {

    constexpr gfx::Vec2 Sheet{ 4096.0f, 512.0f };
    constexpr size_t TL = 0;
    constexpr size_t TR = 1;
    constexpr size_t BR = 2;
    constexpr size_t BL = 3;

    gfx::SpriteDraw sprite(const gfx::Rect dest, const gfx::Rect source)
    {
        gfx::SpriteDraw draw;
        draw.texture = static_cast<gfx::TextureHandle>(1);
        draw.dest = dest;
        draw.source = source;

        return draw;
    }

}

TEST(QuadBuilder, AxisAlignedCornersFollowDest)
{
    const gfx::Quad q = gfx::buildQuad(sprite({ 10.0f, 20.0f, 30.0f, 40.0f }, {}), Sheet);

    EXPECT_FLOAT_EQ(q[TL].x, 10.0f); EXPECT_FLOAT_EQ(q[TL].y, 20.0f);
    EXPECT_FLOAT_EQ(q[TR].x, 40.0f); EXPECT_FLOAT_EQ(q[TR].y, 20.0f);
    EXPECT_FLOAT_EQ(q[BR].x, 40.0f); EXPECT_FLOAT_EQ(q[BR].y, 60.0f);
    EXPECT_FLOAT_EQ(q[BL].x, 10.0f); EXPECT_FLOAT_EQ(q[BL].y, 60.0f);
}

TEST(QuadBuilder, SourceRectBecomesNormalisedUv)
{
    const gfx::Quad q = gfx::buildQuad(sprite({ 0.0f, 0.0f, 128.0f, 128.0f }, { 1024.0f, 0.0f, 512.0f, 512.0f }), Sheet);

    EXPECT_FLOAT_EQ(q[TL].u, 0.25f);  EXPECT_FLOAT_EQ(q[TL].v, 0.0f);
    EXPECT_FLOAT_EQ(q[BR].u, 0.375f); EXPECT_FLOAT_EQ(q[BR].v, 1.0f);
    EXPECT_FLOAT_EQ(q[TR].u, 0.375f); EXPECT_FLOAT_EQ(q[BL].u, 0.25f);
}

TEST(QuadBuilder, EmptySourceMeansWholeTexture)
{
    for (const gfx::Rect source : { gfx::Rect{}, gfx::Rect{ 5.0f, 5.0f, -1.0f, 10.0f }, gfx::Rect{ 5.0f, 5.0f, 10.0f, 0.0f } }) {
        const gfx::Quad q = gfx::buildQuad(sprite({ 0.0f, 0.0f, 8.0f, 8.0f }, source), Sheet);

        EXPECT_FLOAT_EQ(q[TL].u, 0.0f); EXPECT_FLOAT_EQ(q[TL].v, 0.0f);
        EXPECT_FLOAT_EQ(q[BR].u, 1.0f); EXPECT_FLOAT_EQ(q[BR].v, 1.0f);
    }
}

TEST(QuadBuilder, FlipXSwapsHorizontalUv)
{
    gfx::SpriteDraw draw = sprite({ 0.0f, 0.0f, 128.0f, 128.0f }, { 1024.0f, 0.0f, 512.0f, 512.0f });
    draw.flipX = true;
    const gfx::Quad q = gfx::buildQuad(draw, Sheet);

    EXPECT_FLOAT_EQ(q[TL].u, 0.375f);
    EXPECT_FLOAT_EQ(q[TR].u, 0.25f);
    EXPECT_FLOAT_EQ(q[TL].x, 0.0f);     // positions are not mirrored, only the texture
    EXPECT_FLOAT_EQ(q[TL].v, 0.0f);
}

TEST(QuadBuilder, TintIsCopiedToEveryCorner)
{
    gfx::SpriteDraw draw = sprite({ 0.0f, 0.0f, 8.0f, 8.0f }, {});
    draw.tint = gfx::Color{ 10, 20, 30, 40 };

    for (const gfx::QuadVertex& v : gfx::buildQuad(draw, Sheet)) {
        EXPECT_EQ(v.r, 10); EXPECT_EQ(v.g, 20); EXPECT_EQ(v.b, 30); EXPECT_EQ(v.a, 40);
    }
}

TEST(QuadBuilder, QuarterTurnIsClockwiseInYDown)
{
    gfx::SpriteDraw draw = sprite({ 0.0f, 0.0f, 10.0f, 10.0f }, {});
    draw.rotation = std::numbers::pi_v<float> * 0.5f;
    const gfx::Quad q = gfx::buildQuad(draw, Sheet);

    EXPECT_NEAR(q[TL].x, 10.0f, 1e-4f); EXPECT_NEAR(q[TL].y, 0.0f, 1e-4f);
    EXPECT_NEAR(q[TR].x, 10.0f, 1e-4f); EXPECT_NEAR(q[TR].y, 10.0f, 1e-4f);
    EXPECT_NEAR(q[BR].x, 0.0f, 1e-4f);  EXPECT_NEAR(q[BR].y, 10.0f, 1e-4f);
    EXPECT_NEAR(q[BL].x, 0.0f, 1e-4f);  EXPECT_NEAR(q[BL].y, 0.0f, 1e-4f);
}

TEST(QuadBuilder, FlipAndRotationCombine)
{
    gfx::SpriteDraw draw = sprite({ 0.0f, 0.0f, 10.0f, 10.0f }, {});
    draw.rotation = std::numbers::pi_v<float> * 0.5f;
    draw.flipX = true;
    const gfx::Quad q = gfx::buildQuad(draw, Sheet);

    EXPECT_NEAR(q[TL].x, 10.0f, 1e-4f); EXPECT_NEAR(q[TL].y, 0.0f, 1e-4f);
    EXPECT_FLOAT_EQ(q[TL].u, 1.0f);
    EXPECT_FLOAT_EQ(q[TR].u, 0.0f);
}

TEST(QuadBuilder, RectUsesFullUvAndItsColor)
{
    const gfx::RectDraw rect{ gfx::Rect{ 1.0f, 2.0f, 3.0f, 4.0f }, gfx::Color{ 9, 8, 7, 6 }, 0, gfx::Space::Screen };
    const gfx::Quad q = gfx::buildQuad(rect);

    EXPECT_FLOAT_EQ(q[TL].x, 1.0f); EXPECT_FLOAT_EQ(q[BR].y, 6.0f);
    EXPECT_FLOAT_EQ(q[TL].u, 0.0f); EXPECT_FLOAT_EQ(q[BR].u, 1.0f);
    EXPECT_FLOAT_EQ(q[TL].v, 0.0f); EXPECT_FLOAT_EQ(q[BR].v, 1.0f);
    for (const gfx::QuadVertex& v : q) {
        EXPECT_EQ(v.r, 9); EXPECT_EQ(v.a, 6);
    }
}

TEST(QuadBuilder, ZeroTextureSizeFallsBackToWholeTexture)
{
    const gfx::Quad q = gfx::buildQuad(sprite({ 0.0f, 0.0f, 8.0f, 8.0f }, { 0.0f, 0.0f, 10.0f, 10.0f }), gfx::Vec2{ 0.0f, 0.0f });

    for (const gfx::QuadVertex& v : q) {
        EXPECT_TRUE(std::isfinite(v.u) && std::isfinite(v.v));
    }
    EXPECT_FLOAT_EQ(q[TL].u, 0.0f);
    EXPECT_FLOAT_EQ(q[BR].u, 1.0f);
}

TEST(QuadBuilder, IndicesFormTwoTrianglesOverTheCorners)
{
    EXPECT_EQ(gfx::QuadIndices, (std::array<uint16_t, 6>{ 0, 1, 2, 0, 2, 3 }));
}
```

- [ ] **Step 3: Human builds `renderer_tests` — expect failure**

Expected: link error `undefined reference to gfx::buildQuad(...)` (both overloads).

- [ ] **Step 4: Implement**

`Renderer/QuadBuilder/QuadBuilder.cpp`, replace the body:

```cpp
#include "Renderer/QuadBuilder/QuadBuilder.hpp"

#include <cmath>
#include <utility>

namespace gfx {

    namespace {
        Quad corners(const Rect& dest, const float rotation, const Rect& uv, const Color color)
        {
            const float halfW = dest.w * 0.5f;
            const float halfH = dest.h * 0.5f;
            const float centerX = dest.x + halfW;
            const float centerY = dest.y + halfH;
            const float cosine = std::cos(rotation);
            const float sine = std::sin(rotation);

            const std::array<Vec2, 4> offsets{ { { -halfW, -halfH }, { halfW, -halfH }, { halfW, halfH }, { -halfW, halfH } } };
            const std::array<Vec2, 4> uvs{ { { uv.x, uv.y }, { uv.w, uv.y }, { uv.w, uv.h }, { uv.x, uv.h } } };

            Quad quad;
            for (size_t i = 0; i < quad.size(); ++i) {
                // y points down, so the textbook rotation turns clockwise on screen, like SDL.
                quad[i].x = centerX + offsets[i].x * cosine - offsets[i].y * sine;
                quad[i].y = centerY + offsets[i].x * sine + offsets[i].y * cosine;
                quad[i].u = uvs[i].x;
                quad[i].v = uvs[i].y;
                quad[i].r = color.r;
                quad[i].g = color.g;
                quad[i].b = color.b;
                quad[i].a = color.a;
            }

            return quad;
        }
    }

    Quad buildQuad(const SpriteDraw& draw, const Vec2 textureSize)
    {
        // Rect reused as (u0, v0, u1, v1).
        Rect uv{ 0.0f, 0.0f, 1.0f, 1.0f };

        const bool wholeTexture = draw.source.w <= 0.0f || draw.source.h <= 0.0f;
        if (!wholeTexture && textureSize.x > 0.0f && textureSize.y > 0.0f) {
            uv = Rect{
                draw.source.x / textureSize.x,
                draw.source.y / textureSize.y,
                (draw.source.x + draw.source.w) / textureSize.x,
                (draw.source.y + draw.source.h) / textureSize.y
            };
        }

        if (draw.flipX) {
            std::swap(uv.x, uv.w);
        }

        return corners(draw.dest, draw.rotation, uv, draw.tint);
    }

    Quad buildQuad(const RectDraw& draw)
    {
        return corners(draw.dest, 0.0f, Rect{ 0.0f, 0.0f, 1.0f, 1.0f }, draw.color);
    }

}
```

- [ ] **Step 5: Human builds; agent runs the tests**

Agent: `cmake-build-debug/renderer_tests --gtest_filter='QuadBuilder.*'` → `[  PASSED  ] 10 tests.`; full run `--gtest_brief=1` → `[  PASSED  ] 39 tests.`

- [ ] **Step 6: Hand off for commit**

Suggested message: `feat(renderer): QuadBuilder turns draws into GPU-ready quads`.

---

### Task 3: Sprite shaders and BgfxProgram (triangle removed)

**Files:**
- Create: `shaders/vs_sprite.sc`, `shaders/fs_sprite.sc`
- Modify: `shaders/varying.def.sc`
- Delete: `shaders/vs_simple.sc`, `shaders/fs_simple.sc`, `Renderer/Renderer/Renderer.hpp`, `Renderer/Renderer/Renderer.cpp` (plain `rm`; the human stages deletions)
- Create: `Renderer/BgfxProgram/BgfxProgram.hpp`, `Renderer/BgfxProgram/BgfxProgram.cpp`
- Modify: `CMakeLists.txt` (shader lists, `renderer_wrapper` sources)

**Interfaces:**
- Consumes: generated headers `generated/shaders/{glsl,spirv}/{vs,fs}_sprite.sc.bin.h` defining `vs_sprite_glsl`, `vs_sprite_spv`, `fs_sprite_glsl`, `fs_sprite_spv` (`static const uint8_t[]`).
- Produces: `class gfx::BgfxProgram { BgfxProgram(); ~BgfxProgram(); bool valid() const; bgfx::ProgramHandle handle() const; }` — non-copyable; constructed only after `bgfx::init`, destroyed before `bgfx::shutdown`.

- [ ] **Step 1: Shaders**

`shaders/varying.def.sc` (whole file):

```
vec2 v_texcoord0 : TEXCOORD0;
vec4 v_color0    : COLOR0;

vec2 a_position  : POSITION;
vec2 a_texcoord0 : TEXCOORD0;
vec4 a_color0    : COLOR0;
```

`shaders/vs_sprite.sc`:

```
$input a_position, a_texcoord0, a_color0
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

void main()
{
    gl_Position = mul(u_modelViewProj, vec4(a_position, 0.0, 1.0));
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
}
```

`shaders/fs_sprite.sc`:

```
$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

void main()
{
    gl_FragColor = texture2D(s_texColor, v_texcoord0) * v_color0;
}
```

Delete `shaders/vs_simple.sc` and `shaders/fs_simple.sc`.

- [ ] **Step 2: CMake shader lists**

In both `bgfx_compile_shaders(...)` calls replace the `SHADERS` line:

```cmake
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/vs_sprite.sc
```

```cmake
        SHADERS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/fs_sprite.sc
```

In `add_executable(renderer_wrapper ...)` replace

```cmake
        Renderer/Renderer/Renderer.cpp
        Renderer/Renderer/Renderer.hpp
```

with

```cmake
        Renderer/BgfxProgram/BgfxProgram.cpp
        Renderer/BgfxProgram/BgfxProgram.hpp
```

Delete `Renderer/Renderer/Renderer.hpp` and `Renderer/Renderer/Renderer.cpp`.

- [ ] **Step 3: BgfxProgram**

`Renderer/BgfxProgram/BgfxProgram.hpp` (banner, description `RAII sprite shader program, embedded for the API bgfx picked.`):

```cpp
#ifndef RENDERER_WRAPPER_BGFXPROGRAM_HPP
#define RENDERER_WRAPPER_BGFXPROGRAM_HPP

#include <bgfx/bgfx.h>

namespace gfx {

    // Construct after bgfx::init() and destroy before bgfx::shutdown(); check valid().
    class BgfxProgram {
        public:
            BgfxProgram();
            ~BgfxProgram();

            BgfxProgram(const BgfxProgram&) = delete;
            BgfxProgram& operator=(const BgfxProgram&) = delete;

            bool valid() const;
            bgfx::ProgramHandle handle() const;

        private:
            bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    };

}

#endif //RENDERER_WRAPPER_BGFXPROGRAM_HPP
```

`Renderer/BgfxProgram/BgfxProgram.cpp` (banner, same description):

```cpp
#include "Renderer/BgfxProgram/BgfxProgram.hpp"

#include <cstdint>
#include <iostream>
#include <span>

// Generated at build time by shaderc (see CMakeLists.txt).
#include <generated/shaders/glsl/vs_sprite.sc.bin.h>
#include <generated/shaders/glsl/fs_sprite.sc.bin.h>
#include <generated/shaders/spirv/vs_sprite.sc.bin.h>
#include <generated/shaders/spirv/fs_sprite.sc.bin.h>

namespace gfx {

    namespace {
        // A span built from the generated arrays keeps their size; a raw pointer would lose it.
        struct ShaderVariants {
            std::span<const uint8_t> glsl;
            std::span<const uint8_t> spirv;
        };

        // Not BGFX_EMBEDDED_SHADER: on Linux it also references dxbc/essl symbols we never compile.
        bgfx::ShaderHandle createEmbeddedShader(const ShaderVariants& variants, const char* name)
        {
            std::span<const uint8_t> blob;
            switch (bgfx::getRendererType()) {
                case bgfx::RendererType::OpenGL:
                    blob = variants.glsl;
                    break;
                case bgfx::RendererType::Vulkan:
                    blob = variants.spirv;
                    break;
                default:
                    break;
            }

            if (blob.empty()) {
                std::cerr << "[BgfxProgram] ERROR: no embedded '" << name << "' shader for "
                          << bgfx::getRendererName(bgfx::getRendererType()) << "\n";
                return BGFX_INVALID_HANDLE;
            }

            const bgfx::ShaderHandle handle =
                bgfx::createShader(bgfx::makeRef(blob.data(), static_cast<uint32_t>(blob.size())));
            if (bgfx::isValid(handle)) {
                bgfx::setName(handle, name);
            }

            return handle;
        }
    }

    BgfxProgram::BgfxProgram()
    {
        const bgfx::ShaderHandle vertex = createEmbeddedShader({ vs_sprite_glsl, vs_sprite_spv }, "vs_sprite");
        const bgfx::ShaderHandle fragment = createEmbeddedShader({ fs_sprite_glsl, fs_sprite_spv }, "fs_sprite");

        if (!bgfx::isValid(vertex) || !bgfx::isValid(fragment)) {
            if (bgfx::isValid(vertex)) {
                bgfx::destroy(vertex);
            }
            if (bgfx::isValid(fragment)) {
                bgfx::destroy(fragment);
            }
            return;
        }

        m_program = bgfx::createProgram(vertex, fragment, true /* destroy shaders with program */);
        if (!bgfx::isValid(m_program)) {
            std::cerr << "[BgfxProgram] ERROR: bgfx::createProgram failed\n";
        }
    }

    BgfxProgram::~BgfxProgram()
    {
        if (bgfx::isValid(m_program)) {
            bgfx::destroy(m_program);
        }
    }

    bool BgfxProgram::valid() const
    {
        return bgfx::isValid(m_program);
    }

    bgfx::ProgramHandle BgfxProgram::handle() const
    {
        return m_program;
    }

}
```

- [ ] **Step 4: Human builds everything**

Human: Reload CMake, Build ▸ Build Project; paste output.
Expected: shaderc compiles `vs_sprite`/`fs_sprite` for `430` and `spirv`; `renderer_wrapper` and `renderer_tests` link with no warnings. `cmake-build-debug/generated/shaders/glsl/vs_sprite.sc.bin.h` exists. Playground still runs on SDL exactly as before (it does not use bgfx yet).

- [ ] **Step 5: Hand off for commit**

Suggested message: `refactor(bgfx): sprite shaders and BgfxProgram replace the test triangle`.

---

### Task 4: BgfxRenderer, factory and `--backend` flag

**Files:**
- Create: `Renderer/BgfxRenderer/BgfxRenderer.hpp`, `Renderer/BgfxRenderer/BgfxRenderer.cpp`
- Modify: `Renderer/RendererFactory/RendererFactory.cpp`, `main.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `gfx::IRenderer` and types; `gfx::cameraMatrix`, `gfx::identityMatrix`, `gfx::Mat4` (Task 1); `gfx::buildQuad`, `gfx::Quad`, `gfx::QuadVertex`, `gfx::QuadIndices` (Task 2); `gfx::BgfxProgram` (Task 3); `gfx::nativeWindowFromSdl`, `gfx::NativeWindow`, `gfx::NativeWindowKind` (`Renderer/SdlWindowBridge/SdlWindowBridge.hpp`).
- Produces: `static std::unique_ptr<gfx::BgfxRenderer> gfx::BgfxRenderer::create(SDL_Window* window, uint32_t width, uint32_t height);` plus every `IRenderer` override.

- [ ] **Step 1: Header**

`Renderer/BgfxRenderer/BgfxRenderer.hpp` (banner, description `bgfx backend: batched textured quads, camera as a GPU matrix.`):

```cpp
#ifndef RENDERER_WRAPPER_BGFXRENDERER_HPP
#define RENDERER_WRAPPER_BGFXRENDERER_HPP

#include "Renderer/IRenderer.hpp"
#include "Renderer/BgfxProgram/BgfxProgram.hpp"
#include "Renderer/CameraMatrix/CameraMatrix.hpp"

#include <bgfx/bgfx.h>

#include <array>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct SDL_Window;

namespace gfx {

    class BgfxRenderer final : public IRenderer {
        public:
            // Returns nullptr on failure, after logging the reason. bgfx is global: one instance at a time.
            static std::unique_ptr<BgfxRenderer> create(SDL_Window* window, uint32_t width, uint32_t height);

            ~BgfxRenderer() override;

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
                RectDraw rect;
                SpriteDraw sprite;
            };

            struct Texture {
                bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
                Vec2 size;
            };

            BgfxRenderer(uint32_t width, uint32_t height);

            bool createResources();
            void submit(const Command& command, Layer layer);
            void submitLayer(const std::vector<Command>& commands, const Mat4& camera, const Mat4& identity);
            void submitBatch(const Command* first, uint32_t count, const Mat4& camera, const Mat4& identity);
            const Texture* find(TextureHandle texture) const;
            void releasePending();

            inline static bool s_alive = false;

            std::unique_ptr<BgfxProgram> m_program;
            bgfx::UniformHandle m_sampler = BGFX_INVALID_HANDLE;
            bgfx::TextureHandle m_white = BGFX_INVALID_HANDLE;
            bgfx::VertexLayout m_layout;
            std::string m_name;

            uint32_t m_width = 0;
            uint32_t m_height = 0;
            Camera2D m_camera;
            Color m_clear;
            bool m_inFrame = false;

            std::array<std::vector<Command>, LayerCount> m_layers{};
            std::unordered_map<uint32_t, Texture> m_textures;
            uint32_t m_nextTexture = 1;
            std::vector<TextureHandle> m_pendingDestroy;

            bool m_warnedOutsideFrame = false;
            bool m_warnedLayerRange = false;
            bool m_warnedInvalidTexture = false;
            bool m_warnedTransient = false;
    };

}

#endif //RENDERER_WRAPPER_BGFXRENDERER_HPP
```

- [ ] **Step 2: Implementation**

`Renderer/BgfxRenderer/BgfxRenderer.cpp` (banner, same description):

```cpp
#include "Renderer/BgfxRenderer/BgfxRenderer.hpp"

#include "Renderer/QuadBuilder/QuadBuilder.hpp"
#include "Renderer/SdlWindowBridge/SdlWindowBridge.hpp"

#include <bx/math.h>

#include <cstring>
#include <iostream>

namespace gfx {

    namespace {
        constexpr bgfx::ViewId SceneView = 0;
        constexpr uint32_t ResetFlags = BGFX_RESET_VSYNC;
        constexpr uint32_t SamplerFlags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        // No depth: order is submission order inside one sequential view.
        constexpr uint64_t DrawState = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA;
        // 16-bit indices address at most 65 536 vertices per submit.
        constexpr uint32_t MaxQuadsPerBatch = 65536 / 4;

        uint32_t toRgba(const Color color)
        {
            return (static_cast<uint32_t>(color.r) << 24) | (static_cast<uint32_t>(color.g) << 16)
                 | (static_cast<uint32_t>(color.b) << 8) | static_cast<uint32_t>(color.a);
        }
    }

    std::unique_ptr<BgfxRenderer> BgfxRenderer::create(SDL_Window* window, const uint32_t width, const uint32_t height)
    {
        if (s_alive) {
            std::cerr << "[BgfxRenderer] ERROR: bgfx is already running; only one BgfxRenderer may exist\n";
            return nullptr;
        }

        const NativeWindow native = nativeWindowFromSdl(window);
        if (native.nwh == nullptr) {
            return nullptr;
        }

        // renderFrame() before init() on this thread keeps bgfx single-threaded.
        bgfx::renderFrame();

        bgfx::Init init;
        init.type = bgfx::RendererType::Count;     // let bgfx pick the platform default
        init.reset = ResetFlags;
        init.platformData.type = native.kind == NativeWindowKind::Wayland
            ? bgfx::NativeWindowHandleType::Wayland
            : bgfx::NativeWindowHandleType::Default;
        init.swapChain.nwh = native.nwh;
        init.swapChain.ndt = native.ndt;
        init.swapChain.width = width;
        init.swapChain.height = height;

        if (!bgfx::init(init)) {
            std::cerr << "[BgfxRenderer] ERROR: bgfx::init failed\n";
            return nullptr;
        }

        // From here the destructor owns bgfx::shutdown(), including on the failure path below.
        std::unique_ptr<BgfxRenderer> renderer(new BgfxRenderer(width, height));
        if (!renderer->createResources()) {
            return nullptr;
        }

        return renderer;
    }

    BgfxRenderer::BgfxRenderer(const uint32_t width, const uint32_t height)
        : m_width(width)
        , m_height(height)
    {
        s_alive = true;

        m_name = std::string("bgfx / ") + bgfx::getRendererName(bgfx::getRendererType());
        m_camera.center = Vec2{ static_cast<float>(width) * 0.5f, static_cast<float>(height) * 0.5f };

        m_layout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();

        std::cerr << "[BgfxRenderer] ready: " << m_name << " " << width << "x" << height << "\n";
    }

    BgfxRenderer::~BgfxRenderer()
    {
        for (const auto& [id, texture] : m_textures) {
            bgfx::destroy(texture.handle);
        }
        m_textures.clear();

        if (bgfx::isValid(m_white)) {
            bgfx::destroy(m_white);
        }
        if (bgfx::isValid(m_sampler)) {
            bgfx::destroy(m_sampler);
        }
        m_program.reset();

        bgfx::shutdown();
        s_alive = false;
    }

    bool BgfxRenderer::createResources()
    {
        if (m_layout.getStride() != sizeof(QuadVertex)) {
            std::cerr << "[BgfxRenderer] ERROR: vertex layout stride " << m_layout.getStride()
                      << " != sizeof(QuadVertex) " << sizeof(QuadVertex) << "\n";
            return false;
        }

        m_program = std::make_unique<BgfxProgram>();
        if (!m_program->valid()) {
            return false;
        }

        m_sampler = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);

        const uint32_t white = 0xFFFFFFFFu;
        m_white = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, SamplerFlags,
            bgfx::copy(&white, sizeof(white)));

        if (!bgfx::isValid(m_sampler) || !bgfx::isValid(m_white)) {
            std::cerr << "[BgfxRenderer] ERROR: could not create the sampler or the white texture\n";
            return false;
        }

        return true;
    }

    void BgfxRenderer::resize(const uint32_t width, const uint32_t height)
    {
        if (width == 0 || height == 0 || (width == m_width && height == m_height)) {
            return;
        }

        m_width = width;
        m_height = height;

        // Only the fields set here change; nwh/ndt stay the ones bgfx was given.
        bgfx::SwapChain swapChain;
        swapChain.width = width;
        swapChain.height = height;
        bgfx::reset(ResetFlags, &swapChain);
    }

    void BgfxRenderer::beginFrame(const Color clear)
    {
        if (m_inFrame) {
            std::cerr << "[BgfxRenderer] WARN: beginFrame() called twice; ignoring\n";
            return;
        }

        for (auto& layer : m_layers) {
            layer.clear();
        }

        m_clear = clear;
        m_inFrame = true;
    }

    void BgfxRenderer::endFrame()
    {
        if (!m_inFrame) {
            std::cerr << "[BgfxRenderer] WARN: endFrame() without beginFrame(); ignoring\n";
            return;
        }

        const auto width = static_cast<float>(m_width);
        const auto height = static_cast<float>(m_height);

        bgfx::setViewClear(SceneView, BGFX_CLEAR_COLOR, toRgba(m_clear));
        bgfx::setViewRect(SceneView, 0, 0, static_cast<uint16_t>(m_width), static_cast<uint16_t>(m_height));
        bgfx::setViewMode(SceneView, bgfx::ViewMode::Sequential);

        float view[16];
        float projection[16];
        bx::mtxIdentity(view);
        // bottom = height, top = 0: y grows downward, like SDL and the world.
        bx::mtxOrtho(projection, 0.0f, width, height, 0.0f, 0.0f, 1.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(SceneView, view, projection);
        bgfx::touch(SceneView);   // clear even when nothing is drawn

        const Mat4 camera = cameraMatrix(m_camera, Vec2{ width, height });
        const Mat4 identity = identityMatrix();
        for (const auto& layer : m_layers) {
            submitLayer(layer, camera, identity);
        }

        bgfx::frame();
        m_inFrame = false;
        releasePending();
    }

    void BgfxRenderer::setCamera(const Camera2D& camera)
    {
        m_camera = camera;
    }

    TextureHandle BgfxRenderer::createTexture(const uint8_t* rgba, const uint32_t width, const uint32_t height)
    {
        if (rgba == nullptr || width == 0 || height == 0) {
            std::cerr << "[BgfxRenderer] ERROR: createTexture called with empty pixels\n";
            return TextureHandle::Invalid;
        }

        const uint32_t maxSize = bgfx::getCaps()->limits.maxTextureSize;
        if (width > maxSize || height > maxSize) {
            std::cerr << "[BgfxRenderer] ERROR: texture " << width << "x" << height
                      << " exceeds the GPU limit of " << maxSize << "\n";
            return TextureHandle::Invalid;
        }

        // copy(): bgfx uploads on a later frame, after the caller may have freed its pixels.
        const bgfx::TextureHandle handle = bgfx::createTexture2D(static_cast<uint16_t>(width),
            static_cast<uint16_t>(height), false, 1, bgfx::TextureFormat::RGBA8, SamplerFlags,
            bgfx::copy(rgba, width * height * 4));
        if (!bgfx::isValid(handle)) {
            std::cerr << "[BgfxRenderer] ERROR: bgfx::createTexture2D failed\n";
            return TextureHandle::Invalid;
        }

        const uint32_t id = m_nextTexture++;
        m_textures.emplace(id, Texture{ handle, Vec2{ static_cast<float>(width), static_cast<float>(height) } });

        return static_cast<TextureHandle>(id);
    }

    void BgfxRenderer::destroyTexture(const TextureHandle texture)
    {
        if (find(texture) == nullptr) {
            if (!m_warnedInvalidTexture) {
                std::cerr << "[BgfxRenderer] WARN: destroyTexture on an unknown handle\n";
                m_warnedInvalidTexture = true;
            }
            return;
        }

        // Draws are only submitted in endFrame(), so the texture must outlive this frame.
        if (m_inFrame) {
            m_pendingDestroy.push_back(texture);
            return;
        }

        const auto id = static_cast<uint32_t>(texture);
        bgfx::destroy(m_textures[id].handle);
        m_textures.erase(id);
    }

    Vec2 BgfxRenderer::textureSize(const TextureHandle texture) const
    {
        const Texture* found = find(texture);

        return found != nullptr ? found->size : Vec2{ 0.0f, 0.0f };
    }

    void BgfxRenderer::drawRect(const RectDraw& draw)
    {
        Command command;
        command.kind = Command::Kind::Rect;
        command.rect = draw;

        submit(command, draw.layer);
    }

    void BgfxRenderer::drawSprite(const SpriteDraw& draw)
    {
        if (find(draw.texture) == nullptr) {
            if (!m_warnedInvalidTexture) {
                std::cerr << "[BgfxRenderer] WARN: drawSprite with an invalid texture handle\n";
                m_warnedInvalidTexture = true;
            }
            return;
        }

        Command command;
        command.kind = Command::Kind::Sprite;
        command.sprite = draw;

        submit(command, draw.layer);
    }

    const char* BgfxRenderer::backendName() const
    {
        return m_name.c_str();
    }

    void BgfxRenderer::submit(const Command& command, const Layer layer)
    {
        if (!m_inFrame) {
            if (!m_warnedOutsideFrame) {
                std::cerr << "[BgfxRenderer] WARN: draw outside beginFrame()/endFrame(); ignoring\n";
                m_warnedOutsideFrame = true;
            }
            return;
        }

        Layer clamped = layer;
        if (clamped < MinLayer || clamped > MaxLayer) {
            if (!m_warnedLayerRange) {
                std::cerr << "[BgfxRenderer] WARN: layer " << layer << " out of range, clamping\n";
                m_warnedLayerRange = true;
            }
            clamped = clamped < MinLayer ? MinLayer : MaxLayer;
        }

        m_layers[static_cast<size_t>(clamped)].push_back(command);
    }

    void BgfxRenderer::submitLayer(const std::vector<Command>& commands, const Mat4& camera, const Mat4& identity)
    {
        const auto textureOf = [](const Command& c) {
            return c.kind == Command::Kind::Rect ? TextureHandle::Invalid : c.sprite.texture;
        };
        const auto spaceOf = [](const Command& c) {
            return c.kind == Command::Kind::Rect ? c.rect.space : c.sprite.space;
        };

        size_t start = 0;
        while (start < commands.size()) {
            size_t end = start + 1;
            while (end < commands.size() && end - start < MaxQuadsPerBatch
                && textureOf(commands[end]) == textureOf(commands[start])
                && spaceOf(commands[end]) == spaceOf(commands[start])) {
                ++end;
            }

            submitBatch(&commands[start], static_cast<uint32_t>(end - start), camera, identity);
            start = end;
        }
    }

    void BgfxRenderer::submitBatch(const Command* first, const uint32_t count, const Mat4& camera, const Mat4& identity)
    {
        const bool isRect = first->kind == Command::Kind::Rect;
        const Texture* texture = isRect ? nullptr : find(first->sprite.texture);
        if (!isRect && texture == nullptr) {
            return;
        }

        const uint32_t vertexCount = count * 4;
        const uint32_t indexCount = count * 6;
        if (bgfx::getAvailTransientVertexBuffer(vertexCount, m_layout) < vertexCount
            || bgfx::getAvailTransientIndexBuffer(indexCount) < indexCount) {
            if (!m_warnedTransient) {
                std::cerr << "[BgfxRenderer] WARN: transient buffers exhausted; dropping quads this frame\n";
                m_warnedTransient = true;
            }
            return;
        }

        bgfx::TransientVertexBuffer vertices;
        bgfx::TransientIndexBuffer indices;
        if (!bgfx::allocTransientBuffers(&vertices, m_layout, vertexCount, &indices, indexCount)) {
            return;
        }

        for (uint32_t i = 0; i < count; ++i) {
            const Command& command = first[i];
            const Quad quad = isRect ? buildQuad(command.rect) : buildQuad(command.sprite, texture->size);
            std::memcpy(vertices.data + static_cast<size_t>(i) * sizeof(Quad), quad.data(), sizeof(Quad));

            std::array<uint16_t, 6> quadIndices = QuadIndices;
            for (uint16_t& index : quadIndices) {
                index = static_cast<uint16_t>(index + i * 4);
            }
            std::memcpy(indices.data + static_cast<size_t>(i) * sizeof(quadIndices), quadIndices.data(), sizeof(quadIndices));
        }

        const Space space = isRect ? first->rect.space : first->sprite.space;
        bgfx::setTransform(space == Space::World ? camera.data() : identity.data());
        bgfx::setVertexBuffer(0, &vertices);
        bgfx::setIndexBuffer(&indices);
        bgfx::setTexture(0, m_sampler, isRect ? m_white : texture->handle);
        bgfx::setState(DrawState);
        bgfx::submit(SceneView, m_program->handle());
    }

    const BgfxRenderer::Texture* BgfxRenderer::find(const TextureHandle texture) const
    {
        const auto found = m_textures.find(static_cast<uint32_t>(texture));

        return found != m_textures.end() ? &found->second : nullptr;
    }

    void BgfxRenderer::releasePending()
    {
        for (const TextureHandle texture : m_pendingDestroy) {
            const auto found = m_textures.find(static_cast<uint32_t>(texture));
            if (found != m_textures.end()) {
                bgfx::destroy(found->second.handle);
                m_textures.erase(found);
            }
        }

        m_pendingDestroy.clear();
    }

}
```

- [ ] **Step 3: CMake and factory**

`CMakeLists.txt`, in `add_executable(renderer_wrapper ...)` after the BgfxProgram lines:

```cmake
        Renderer/BgfxRenderer/BgfxRenderer.cpp
        Renderer/BgfxRenderer/BgfxRenderer.hpp
```

`Renderer/RendererFactory/RendererFactory.cpp`: add `#include "Renderer/BgfxRenderer/BgfxRenderer.hpp"` above the NullRenderer include, and replace the `Backend::Bgfx` case:

```cpp
            case Backend::Bgfx:
                return BgfxRenderer::create(window, width, height);
```

Remove `#include <iostream>`: its only use was the old `Backend::Bgfx` error message.

- [ ] **Step 4: `--backend` flag in main.cpp**

Add `#include <string_view>` to the standard includes. In the anonymous namespace, after `assetPath`:

```cpp
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
```

Replace the start of `main`:

```cpp
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
```

with

```cpp
int main(int argc, char** argv)
{
    const std::optional<gfx::Backend> backend = parseBackend(argc, argv);
    if (!backend.has_value()) {
        return 1;
    }

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
```

and the renderer creation:

```cpp
    std::unique_ptr<gfx::IRenderer> renderer =
        gfx::makeRenderer(gfx::Backend::Sdl3Cpu, window, WindowWidth, WindowHeight);
```

with

```cpp
    std::unique_ptr<gfx::IRenderer> renderer = gfx::makeRenderer(*backend, window, WindowWidth, WindowHeight);
```

- [ ] **Step 5: Human builds and runs the playground on bgfx — full checklist**

Human: Build ▸ Build Project (no warnings), run `renderer_wrapper` with the play button (no arguments); paste the log.

Expected log starts with `[BgfxRenderer] ready: bgfx / OpenGL 1280x720` (or `Vulkan`), then `backend: bgfx / ...`, `[Showcase] textureSize: 4096x512`.

Visual and input checks, compared with an SDL run (`--backend=sdl`) side by side:
1. Scene: ground, 8 platforms, translucent HUD bar, mode button; bull standing on the ground, same size and position as SDL.
2. Edit mode: WASD/arrows pan, wheel zooms.
3. Play mode (Tab or button): A/D walk with animation, flip when walking left, idle frame when stopped, camera follows, wheel ignored.
4. Showcase: spinning bull changes colour above the third platform, rotating **clockwise**; 8-frame strip at the right of the HUD bar, right way up.
5. **R** several times: log `reloaded mid-frame: handle N -> N+1`, no flicker, no warning.
6. **F** once: five `[BgfxRenderer] WARN` lines (outside frame, called twice, layer 99, invalid handle, endFrame without beginFrame); later presses only the two repeating ones.
7. **Resize** the window: HUD bar stretches, strip stays right, button clickable, nothing stretched or upside down.
8. **Minimise and restore:** no error, drawing resumes.
9. Run with `--backend=nope` (edit the run configuration's program arguments): prints usage, exit code 1, no window.
10. Quit with Escape: exit code 0, no bgfx error.

Agent: `cmake-build-debug/renderer_tests --gtest_brief=1` → `[  PASSED  ] 39 tests.`

- [ ] **Step 6: Hand off for commit**

Suggested message: `feat(bgfx): BgfxRenderer behind IRenderer, default backend with --backend=sdl fallback`.

---

### Task 5: Documentation and the SDL run configuration

**Files:**
- Modify: `docs/superpowers/specs/2026-09-21-renderer-interface-design.md`
- Modify: `CLAUDE.md`

**Interfaces:** none (documentation only).

- [ ] **Step 1: Apply the old-spec edits (new spec §10)**

In `2026-09-21-renderer-interface-design.md`:

- §6.1, replace the bgfx bullet with:
  `- **bgfx:** one sequential view per pass; layers are ordered by submission. See `2026-10-04-bgfx-backend-design.md`.`
- §10, replace the `BgfxRenderer` row's Notes cell with:
  `Batched textured quads, camera as a GPU matrix (see 2026-10-04-bgfx-backend-design.md)`
- §11, replace the `Renderer/` entry of the layout block with:
  ```
    QuadBuilder/
      QuadBuilder.hpp/.cpp         draw -> 4 vertices, unit-tested
    CameraMatrix/
      CameraMatrix.hpp/.cpp        camera -> GPU matrix, unit-tested against worldToScreen
    BgfxProgram/
      BgfxProgram.hpp/.cpp         embedded sprite shaders
    BgfxRenderer/
      BgfxRenderer.hpp/.cpp        bgfx backend
  ```
- §13, prefix phases 1-4 with `**Done (2026-10-04).**`.
- §14, delete the row `| bgfx caps view count (256 by default) | ... |`.
- Header: change `**Status:** approved design, not yet implemented` to `**Status:** phases 1-4 implemented; bgfx details in 2026-10-04-bgfx-backend-design.md`.

- [ ] **Step 2: CLAUDE.md layout**

In the `## Layout` code block, replace the `  SdlRenderer/SdlRenderer.{hpp,cpp} has a .cpp -> own directory` line and the `  ...` line after it with:

```
  SdlRenderer/SdlRenderer.{hpp,cpp} has a .cpp -> own directory
  BgfxRenderer/, BgfxProgram/       bgfx backend (default; --backend=sdl for SDL)
  QuadBuilder/, CameraMatrix/       backend-free, in renderer_core, unit-tested
  ...
```

and replace `shaders/ ... (varying.def.sc, vs_simple.sc, fs_simple.sc)` with `(varying.def.sc, vs_sprite.sc, fs_sprite.sc)`.

- [ ] **Step 3: Human adds the SDL run configuration**

Human, in CLion: Run ▸ Edit Configurations ▸ copy `renderer_wrapper` ▸ name `renderer_wrapper (SDL)` ▸ Program arguments `--backend=sdl` ▸ make sure **Before launch** contains **Build**. Run it once: log shows `backend: SDL3 / ...`.

- [ ] **Step 4: Hand off for commit**

Suggested message: `docs: record the bgfx backend in the interface spec and CLAUDE.md`.
