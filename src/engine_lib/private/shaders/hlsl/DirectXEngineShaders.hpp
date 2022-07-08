#pragma once

// Custom.
#include "shaders/ShaderDescription.h"
#include "shaders/ShaderParameter.h"

namespace ne {
    /** Stores engine shader definitions used in DirectX renderer. */
    class DirectXEngineShaders {
    public:
        DirectXEngineShaders() = delete;
        DirectXEngineShaders(const DirectXEngineShaders&) = delete;
        DirectXEngineShaders& operator=(const DirectXEngineShaders&) = delete;

        /** Default vertex shader. */
        static inline const auto vertexShader = ShaderDescription(
            "engine.default.vs",
            "res/engine/shaders/default.hlsl",
            ShaderType::VERTEX_SHADER,
            "vsDefault",
            {});

        /** Default pixel shader. */
        static inline const auto pixelShader = ShaderDescription(
            "engine.default.ps",
            "res/engine/shaders/default.hlsl",
            ShaderType::PIXEL_SHADER,
            "psDefault",
            {});
    };
} // namespace ne
