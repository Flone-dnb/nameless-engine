#pragma once

// Standard.
#include <string>

namespace ne {

    /** Stores unique names of engine shaders. */
    class EngineShaderNames {
    public:
        EngineShaderNames() = delete;

        /** Groups MeshNode's shader names. */
        struct MeshNode {
            /**
             * Returns a globally unique name of the vertex shader used by mesh nodes.
             *
             * @return Shader name.
             */
            static inline std::string getVertexShaderName() { return "engine.meshnode.vs"; }

            /**
             * Returns a globally unique name of the pixel/fragment shader used by mesh nodes.
             *
             * @return Shader name.
             */
            static inline std::string getFragmentShaderName() { return "engine.meshnode.fs"; }
        };

        /** Groups info about shaders used in Forward+ light culling process. */
        struct ForwardPlus {
            /**
             * Returns a globally unique name of the compute shader used to calculate grid of frustums
             * for light culling.
             *
             * @return Shader name.
             */
            static inline std::string getCalculateFrustumGridComputeShaderName() {
                return "engine.fp.frustum_grid.comp";
            }

            /**
             * Returns a globally unique name of the compute shader used to reset global counters for light
             * culling.
             *
             * @return Shader name.
             */
            static inline std::string getPrepareLightCullingComputeShaderName() {
                return "engine.fp.pre_light_culling.comp";
            }

            /**
             * Returns a globally unique name of the compute shader used to do light culling.
             *
             * @return Shader name.
             */
            static inline std::string getLightCullingComputeShaderName() {
                return "engine.fp.light_culling.comp";
            }
        };
    };
} // namespace ne
