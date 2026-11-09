/*
    Created on: 10/4/26
    Filename: BgfxRenderer.cpp
    Description: bgfx backend: batched textured quads and meshes, camera as a GPU matrix.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

#include "Renderer/BgfxRenderer/BgfxRenderer.hpp"

#include "Renderer/QuadBuilder/QuadBuilder.hpp"
#include "Renderer/SdlWindowBridge/SdlWindowBridge.hpp"
#include "Renderer/ShapeBuilder/ShapeBuilder.hpp"

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
        if (m_layout.getStride() != sizeof(Vertex)) {
            std::cerr << "[BgfxRenderer] ERROR: vertex layout stride " << m_layout.getStride()
                      << " != sizeof(Vertex) " << sizeof(Vertex) << "\n";
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
        m_meshVertices.clear();
        m_meshIndices.clear();

        m_clear = clear;
        m_inFrame = true;
        m_dropRestOfFrame = false;
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

    void BgfxRenderer::drawMesh(const MeshDraw& draw)
    {
        if (draw.vertices.empty() || draw.indices.empty()) {
            return;
        }

        // Before the arenas: beginFrame() is the only thing that clears them.
        if (!inFrameOrWarn()) {
            return;
        }

        const bool knownTexture = draw.texture == TextureHandle::Invalid || find(draw.texture) != nullptr;
        if (!isValidMesh(draw.vertices, draw.indices) || !knownTexture) {
            if (!m_warnedInvalidMesh) {
                std::cerr << "[BgfxRenderer] WARN: drawMesh with malformed geometry or an unknown texture; ignoring\n";
                m_warnedInvalidMesh = true;
            }
            return;
        }

        Command command;
        command.kind = Command::Kind::Mesh;
        command.firstVertex = static_cast<uint32_t>(m_meshVertices.size());
        command.vertexCount = static_cast<uint32_t>(draw.vertices.size());
        command.firstIndex = static_cast<uint32_t>(m_meshIndices.size());
        command.indexCount = static_cast<uint32_t>(draw.indices.size());
        command.texture = draw.texture;
        command.space = draw.space;

        m_meshVertices.insert(m_meshVertices.end(), draw.vertices.begin(), draw.vertices.end());
        m_meshIndices.insert(m_meshIndices.end(), draw.indices.begin(), draw.indices.end());

        submit(command, draw.layer);
    }

    const char* BgfxRenderer::backendName() const
    {
        return m_name.c_str();
    }

    TextureHandle BgfxRenderer::textureOf(const Command& command)
    {
        switch (command.kind) {
            case Command::Kind::Rect: return TextureHandle::Invalid;
            case Command::Kind::Sprite: return command.sprite.texture;
            case Command::Kind::Mesh: return command.texture;
        }
        return TextureHandle::Invalid;
    }

    Space BgfxRenderer::spaceOf(const Command& command)
    {
        switch (command.kind) {
            case Command::Kind::Rect: return command.rect.space;
            case Command::Kind::Sprite: return command.sprite.space;
            case Command::Kind::Mesh: return command.space;
        }
        return Space::World;
    }

    uint32_t BgfxRenderer::vertexCountOf(const Command& command)
    {
        return command.kind == Command::Kind::Mesh ? command.vertexCount : 4u;
    }

    uint32_t BgfxRenderer::indexCountOf(const Command& command)
    {
        return command.kind == Command::Kind::Mesh ? command.indexCount : 6u;
    }

    bool BgfxRenderer::inFrameOrWarn()
    {
        if (m_inFrame) {
            return true;
        }

        if (!m_warnedOutsideFrame) {
            std::cerr << "[BgfxRenderer] WARN: draw outside beginFrame()/endFrame(); ignoring\n";
            m_warnedOutsideFrame = true;
        }
        return false;
    }

    void BgfxRenderer::submit(const Command& command, const Layer layer)
    {
        if (!inFrameOrWarn()) {
            return;
        }

        m_layers[static_cast<size_t>(layerSlot(layer))].push_back(command);
    }

    void BgfxRenderer::submitLayer(const std::vector<Command>& commands, const Mat4& camera, const Mat4& identity)
    {
        size_t start = 0;
        while (start < commands.size()) {
            const TextureHandle texture = textureOf(commands[start]);
            const Space space = spaceOf(commands[start]);
            size_t batchVertices = vertexCountOf(commands[start]);

            // 16-bit indices address at most MaxMeshVertices per submit.
            size_t end = start + 1;
            while (end < commands.size()
                && textureOf(commands[end]) == texture
                && spaceOf(commands[end]) == space
                && batchVertices + vertexCountOf(commands[end]) <= MaxMeshVertices) {
                batchVertices += vertexCountOf(commands[end]);
                ++end;
            }

            submitBatch(&commands[start], static_cast<uint32_t>(end - start), camera, identity);
            start = end;
        }
    }

    void BgfxRenderer::submitBatch(const Command* first, const uint32_t count, const Mat4& camera, const Mat4& identity)
    {
        if (m_dropRestOfFrame) {
            return;
        }

        const TextureHandle batchTexture = textureOf(*first);
        const Texture* texture = nullptr;
        if (batchTexture != TextureHandle::Invalid) {
            texture = find(batchTexture);
            if (texture == nullptr) {
                return;
            }
        }

        uint32_t vertexCount = 0;
        uint32_t indexCount = 0;
        for (uint32_t i = 0; i < count; ++i) {
            vertexCount += vertexCountOf(first[i]);
            indexCount += indexCountOf(first[i]);
        }

        bgfx::TransientVertexBuffer vertices;
        bgfx::TransientIndexBuffer indices;
        if (!bgfx::allocTransientBuffers(&vertices, m_layout, vertexCount, &indices, indexCount)) {
            if (!m_warnedTransient) {
                std::cerr << "[BgfxRenderer] WARN: transient buffers exhausted; dropping the rest of this frame\n";
                m_warnedTransient = true;
            }
            m_dropRestOfFrame = true;
            return;
        }

        static_assert(sizeof(Quad) == 4 * sizeof(Vertex), "a Quad must be four packed vertices");

        auto* const indexOut = reinterpret_cast<uint16_t*>(indices.data);

        // Offsets run across the batch: an untextured mesh and a rect can share it.
        uint32_t vertexOffset = 0;
        uint32_t indexOffset = 0;
        for (uint32_t i = 0; i < count; ++i) {
            const Command& command = first[i];
            uint8_t* const vertexOut = vertices.data + static_cast<size_t>(vertexOffset) * sizeof(Vertex);

            if (command.kind == Command::Kind::Mesh) {
                std::memcpy(vertexOut, m_meshVertices.data() + command.firstVertex,
                    static_cast<size_t>(command.vertexCount) * sizeof(Vertex));
                for (uint32_t j = 0; j < command.indexCount; ++j) {
                    indexOut[indexOffset + j] = static_cast<uint16_t>(m_meshIndices[command.firstIndex + j] + vertexOffset);
                }
            } else {
                const Quad quad = command.kind == Command::Kind::Rect
                    ? buildQuad(command.rect)
                    : buildQuad(command.sprite, texture->size);
                std::memcpy(vertexOut, quad.data(), sizeof(Quad));
                for (uint32_t j = 0; j < QuadIndices.size(); ++j) {
                    indexOut[indexOffset + j] = static_cast<uint16_t>(QuadIndices[j] + vertexOffset);
                }
            }

            vertexOffset += vertexCountOf(command);
            indexOffset += indexCountOf(command);
        }

        bgfx::setTransform(spaceOf(*first) == Space::World ? camera.data() : identity.data());
        bgfx::setVertexBuffer(0, &vertices);
        bgfx::setIndexBuffer(&indices);
        bgfx::setTexture(0, m_sampler, texture != nullptr ? texture->handle : m_white);
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
