#pragma once

namespace ne {
    /** Stores unique names of engine shaders. */
    class EngineShaderNames {
    public:
        EngineShaderNames() = delete;
        EngineShaderNames(const EngineShaderNames&) = delete;
        EngineShaderNames& operator=(const EngineShaderNames&) = delete;

        /** Unique name of the engine's vertex shader. */
        static inline const auto sMeshNodeVertexShaderName = "engine.meshnode.vs";

        /** Unique name of the engine's pixel shader. */
        static inline const auto sMeshNodePixelShaderName = "engine.meshnode.ps";
    };
} // namespace ne
