#pragma once

// Custom.
#include "material/ShaderDescription.h"
#include "misc/ProjectPaths.h"
#include "material/EngineShaderNames.hpp"

namespace ne {
    /** Stores engine shader definitions used in Vulkan renderer. */
    class GlslEngineShaders {
    public:
        GlslEngineShaders() = delete;

        /** Mesh node's vertex shader. */
        static inline const auto meshNodeVertexShader = ShaderDescription(
            EngineShaderNames::MeshNode::sVertexShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/glsl/final/MeshNode.vert",
            ShaderType::VERTEX_SHADER,
            "main",
            {});

        /** Mesh node's fragment shader. */
        static inline const auto meshNodeFragmentShader = ShaderDescription(
            EngineShaderNames::MeshNode::sPixelShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/glsl/final/MeshNode.frag",
            ShaderType::PIXEL_SHADER,
            "main",
            {});
    };
} // namespace ne
