#pragma once

namespace ne {
    /** Stores constant predefined macros (that never change) for some engine shaders. */
    class EngineShaderConstantMacros {
    public:
        EngineShaderConstantMacros() = delete;

        /** Groups info about shaders used in Forward+ light culling process. */
        struct ForwardPlus {
            /**
             * Defines how much threads should be executed in the X and the Y dimensions for
             * compute shader that calculates frustums for light grid tiles.
             */
            struct FrustumGridThreadsInGroupXyMacro {
                /** Macro name. */
                static inline const auto sName = "THREADS_IN_GROUP_XY";

                /** Macro value. */
                static inline const auto sValue = "16";
            };
        };
    };
}
