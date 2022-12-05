#pragma once

// Custom.
#include "shaders/ShaderDescription.h"
#include "misc/ProjectPaths.h"

namespace ne {
    /** Stores engine shader definitions used in DirectX renderer. */
    class HlslEngineShaders {
    public:
        HlslEngineShaders() = delete;
        HlslEngineShaders(const HlslEngineShaders&) = delete;
        HlslEngineShaders& operator=(const HlslEngineShaders&) = delete;

        /** Default vertex shader. */
        static inline const auto vertexShader = ShaderDescription(
            "engine.default.vs",
            ProjectPaths::getDirectoryForResources(ResourceDirectory::ENGINE) / "shaders/default.hlsl",
            ShaderType::VERTEX_SHADER,
            "vsDefault",
            {});

        /** Default pixel shader. */
        static inline const auto pixelShader = ShaderDescription(
            "engine.default.ps",
            ProjectPaths::getDirectoryForResources(ResourceDirectory::ENGINE) / "shaders/default.hlsl",
            ShaderType::PIXEL_SHADER,
            "psDefault",
            {});
    };
} // namespace ne
