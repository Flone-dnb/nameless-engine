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

        /** Groups info about shaders used in Forward+ light culling process. */
        struct ForwardPlus {
            /**
             * Globally unique name of the compute shader used to calculate grid of frustums
             * for light culling.
             */
            static inline const auto sCalculateFrustumGridComputeShaderName = "engine.fp.frustum_grid.comp";

            /** Globally unique name of the compute shader used to reset global counters for light culling. */
            static inline const auto sPrepareLightCullingComputeShaderName =
                "engine.fp.pre_light_culling.comp";

            /** Globally unique name of the compute shader used to do light culling. */
            static inline const auto sLightCullingComputeShaderName = "engine.fp.light_culling.comp";
        };
    };
} // namespace ne
