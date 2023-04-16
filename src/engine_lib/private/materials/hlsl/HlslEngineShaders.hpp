#pragma once

// Custom.
#include "materials/ShaderDescription.h"
#include "misc/ProjectPaths.h"
#include "materials/EngineShaderNames.hpp"

namespace ne {
    /** Stores engine shader definitions used in DirectX renderer. */
    class HlslEngineShaders {
    public:
        HlslEngineShaders() = delete;

        /** Mesh node's vertex shader. */
        static inline const auto meshNodeVertexShader = ShaderDescription(
            EngineShaderNames::sMeshNodeVertexShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) / "shaders/hlsl/MeshNode.hlsl",
            ShaderType::VERTEX_SHADER,
            "vsMeshNode",
            {});

        /** Mesh node's pixel shader. */
        static inline const auto meshNodePixelShader = ShaderDescription(
            EngineShaderNames::sMeshNodePixelShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) / "shaders/hlsl/MeshNode.hlsl",
            ShaderType::PIXEL_SHADER,
            "psMeshNode",
            {});
    };
} // namespace ne
