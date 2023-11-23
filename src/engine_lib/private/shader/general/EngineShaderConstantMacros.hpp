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
             *
             * @remark This macro also defines how much pixels there are in one grid tile.
             */
            struct FrustumGridThreadsInGroupXyMacro {
                /** Macro name. */
                static inline const auto sName = "THREADS_IN_GROUP_XY";

                /** Macro value. */
                static inline const auto sValue = "16";
            };

            /**
             * Defines how much point lights are expected to be on average in a light grid
             * tile for opaque or transparent geometry.
             *
             * @remark Determines the size for light lists and light grid.
             */
            struct AveragePointLightNumPerTileMacro {
                /** Macro name. */
                static inline const auto sName = "AVERAGE_POINT_LIGHT_NUM_PER_TILE";

                /** Macro value. */
                static inline const auto sValue = "200";
            };

            /**
             * Defines how much spotlights are expected to be on average in a light grid
             * tile for opaque or transparent geometry.
             *
             * @remark Determines the size for light lists and light grid.
             */
            struct AverageSpotLightNumPerTileMacro {
                /** Macro name. */
                static inline const auto sName = "AVERAGE_SPOT_LIGHT_NUM_PER_TILE";

                /** Macro value. */
                static inline const auto sValue = "200";
            };

            /**
             * Defines how much directional lights are expected to be on average in a light grid
             * tile for opaque or transparent geometry.
             *
             * @remark Determines the size for light lists and light grid.
             */
            struct AverageDirectionalLightNumPerTileMacro {
                /** Macro name. */
                static inline const auto sName = "AVERAGE_DIRECTIONAL_LIGHT_NUM_PER_TILE";

                /** Macro value. */
                static inline const auto sValue = "150";
            };
        };
    };
}
