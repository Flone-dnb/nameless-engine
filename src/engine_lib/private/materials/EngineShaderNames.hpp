#pragma once

namespace ne {

    /** Stores unique names of engine shaders. */
    class EngineShaderNames {
    public:
        EngineShaderNames() = delete;

        struct MeshNode {
            /** Globally unique name of the vertex shader used by MeshNodes. */
            static inline const auto sVertexShaderName = "engine.meshnode.vs";

            /** Globally unique name of the pixel shader used by MeshNodes. */
            static inline const auto sPixelShaderName = "engine.meshnode.ps";

            /** Name of the entry function used in MeshNode's vertex shader. */
            static inline const auto sVertexShaderEntryName = "vsMeshNode";

            /** Name of the entry function used in MeshNode's pixel shader. */
            static inline const auto sPixelShaderEntryName = "psMeshNode";
        };
    };
} // namespace ne
