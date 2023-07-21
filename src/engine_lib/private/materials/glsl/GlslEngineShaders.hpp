#pragma once

// Custom.
#include "materials/ShaderDescription.h"
#include "misc/ProjectPaths.h"
#include "materials/EngineShaderNames.hpp"

namespace ne {
    /** Stores engine shader definitions used in Vulkan renderer. */
    class GlslEngineShaders {
    public:
        GlslEngineShaders() = delete;

        /** Mesh node's vertex shader. */
        static inline const auto meshNodeVertexShader = ShaderDescription(
            EngineShaderNames::sMeshNodeVertexShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) / "shaders/glsl/MeshNode.vert",
            ShaderType::VERTEX_SHADER,
            "main",
            {});

        /** Mesh node's fragment shader. */
        static inline const auto meshNodeFragmentShader = ShaderDescription(
            EngineShaderNames::sMeshNodePixelShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) / "shaders/glsl/MeshNode.frag",
            ShaderType::PIXEL_SHADER,
            "main",
            {});
    };
} // namespace ne
