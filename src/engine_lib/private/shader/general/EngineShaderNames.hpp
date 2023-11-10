#pragma once

namespace ne {

    /** Stores unique names of engine shaders. */
    class EngineShaderNames {
    public:
        EngineShaderNames() = delete;

        /** Groups MeshNode's shader info. */
        struct MeshNode {
            /** Globally unique name of the vertex shader used by MeshNodes. */
            static inline const auto sVertexShaderName = "engine.meshnode.vs";

            /** Globally unique name of the pixel shader used by MeshNodes. */
            static inline const auto sPixelShaderName = "engine.meshnode.ps";
        };
    };
} // namespace ne