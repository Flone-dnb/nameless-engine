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

        // /** Valid combinations of pixel shader macros (settings). */
        // static inline const std::set<std::set<ShaderParameter>>
        // validPixelShaderParameterCombinations =
        //     {{
        //         {ShaderParameter::TEXTURE_FILTERING_POINT},
        //         {ShaderParameter::TEXTURE_FILTERING_LINEAR},
        //         {ShaderParameter::TEXTURE_FILTERING_ANISOTROPIC}, // 3 configs ------------
        //         {ShaderParameter::TEXTURE_FILTERING_POINT,
        //          ShaderParameter::USE_DIFFUSE_TEXTURE},
        //         {ShaderParameter::TEXTURE_FILTERING_LINEAR,
        //          ShaderParameter::USE_DIFFUSE_TEXTURE},
        //         {ShaderParameter::TEXTURE_FILTERING_ANISOTROPIC,
        //          ShaderParameter::USE_DIFFUSE_TEXTURE}, // 6 configs ------------
        //         {ShaderParameter::TEXTURE_FILTERING_POINT,
        //          ShaderParameter::USE_DIFFUSE_TEXTURE,
        //          ShaderParameter::USE_NORMAL_TEXTURE},
        //         {ShaderParameter::TEXTURE_FILTERING_LINEAR,
        //          ShaderParameter::USE_DIFFUSE_TEXTURE,
        //          ShaderParameter::USE_NORMAL_TEXTURE},
        //         {ShaderParameter::TEXTURE_FILTERING_ANISOTROPIC,
        //          ShaderParameter::USE_DIFFUSE_TEXTURE,
        //          ShaderParameter::USE_NORMAL_TEXTURE}, // 9 configs ------------
        //     }};
    };
} // namespace ne
