#pragma once

// Custom.
#include "shaders/ShaderDescription.h"

namespace ne {
    /** Stores DirectX engine shaders. */
    class DirectXEngineShaders {
    public:
        /** Default vertex shader. */
        static inline ShaderDescription vsDefault = {
            "engine.default.vs",
            "res/engine/shaders/default.hlsl",
            ShaderType::VERTEX_SHADER,
            "vsDefault",
            {}};

        /** Default pixel shader (no diffuse texture). */
        static inline ShaderDescription psDefaultNoDiffuseTexture = {
            "engine.default.ps",
            "res/engine/shaders/default.hlsl",
            ShaderType::PIXEL_SHADER,
            "psDefault",
            {}};

        /** Default pixel shader (point texture filtering). */
        static inline ShaderDescription psDefaultTextureFilteringPoint = {
            "engine.default.ps.tfp",
            "res/engine/shaders/default.hlsl",
            ShaderType::PIXEL_SHADER,
            "psDefault",
            {"USE_DIFFUSE_TEXTURE", "TEXTURE_FILTERING_POINT"}};

        /** Default vertex shader (linear texture filtering). */
        static inline ShaderDescription psDefaultTextureFilteringLinear = {
            "engine.default.ps.tfl",
            "res/engine/shaders/default.hlsl",
            ShaderType::PIXEL_SHADER,
            "psDefault",
            {"USE_DIFFUSE_TEXTURE", "TEXTURE_FILTERING_LINEAR"}};

        /** Default vertex shader (point texture filtering). */
        static inline ShaderDescription psDefaultTextureFilteringAnisotropic = {
            "engine.default.ps.tfa",
            "res/engine/shaders/default.hlsl",
            ShaderType::PIXEL_SHADER,
            "psDefault",
            {"USE_DIFFUSE_TEXTURE", "TEXTURE_FILTERING_ANISOTROPIC"}};
    };
} // namespace ne
