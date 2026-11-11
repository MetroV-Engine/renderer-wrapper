/*
    Created on: 10/4/26
    Filename: BgfxProgram.cpp
    Description: RAII sprite shader program, embedded for the API bgfx picked.

    ███╗   ███╗███████╗████████╗██████╗  ██████╗ ██╗   ██╗
    ████╗ ████║██╔════╝╚══██╔══╝██╔══██╗██╔═══██╗██║   ██║
    ██╔████╔██║█████╗     ██║   ██████╔╝██║   ██║██║   ██║
    ██║╚██╔╝██║██╔══╝     ██║   ██╔══██╗██║   ██║╚██╗ ██╔╝
    ██║ ╚═╝ ██║███████╗   ██║   ██║  ██║╚██████╔╝ ╚████╔╝
    ╚═╝     ╚═╝╚══════╝   ╚═╝   ╚═╝  ╚═╝ ╚═════╝   ╚═══╝
*/

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
