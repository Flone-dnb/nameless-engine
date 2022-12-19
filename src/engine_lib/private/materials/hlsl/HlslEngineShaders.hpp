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
        HlslEngineShaders(const HlslEngineShaders&) = delete;
        HlslEngineShaders& operator=(const HlslEngineShaders&) = delete;

        /** Default vertex shader. */
        static inline const auto vertexShader = ShaderDescription(
            EngineShaderNames::sVertexShaderName,
            ProjectPaths::getDirectoryForResources(ResourceDirectory::ENGINE) / "shaders/default.hlsl",
            ShaderType::VERTEX_SHADER,
            "vsDefault",
            {});

        /** Default pixel shader. */
        static inline const auto pixelShader = ShaderDescription(
            EngineShaderNames::sPixelShaderName,
            ProjectPaths::getDirectoryForResources(ResourceDirectory::ENGINE) / "shaders/default.hlsl",
            ShaderType::PIXEL_SHADER,
            "psDefault",
            {});
    };
} // namespace ne
