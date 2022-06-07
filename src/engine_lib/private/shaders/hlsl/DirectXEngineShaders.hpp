#pragma once

// Custom.
#include "shaders/ShaderDescription.h"

namespace ne {
    /** Stores DirectX engine shaders. */
    class DirectXEngineShaders {
    public:
        /** Default vertex shader. */
        static inline ShaderDescription vsDefault = {
            "engine.default", "res/engine/shaders/default.hlsl", ShaderType::VERTEX_SHADER, "VS", {}};
    };
} // namespace ne
