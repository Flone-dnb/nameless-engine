#pragma once

// Standard.
#include <unordered_map>
#include <string>

namespace ne {
    /** Stores constant predefined macros (that never change) for some engine shaders. */
    class EngineShaderConstantMacros {
    public:
        EngineShaderConstantMacros() = delete;

        /** Groups macros used in Forward+ light culling process. */
        struct ForwardPlus {
            /**
             * Returns a macro that defines how much threads should be executed in the X and the
             * Y dimensions for compute shader that calculates frustums for light grid tiles.
             *
             * @remark This macro also defines how much pixels there are in one grid tile.
             *
             * @return Macro name and macro value.
             */
            static inline std::pair<std::string, std::string> getLightGridTileSizeMacro() {
                return {"LIGHT_GRID_TILE_SIZE_IN_PIXELS", "16"};
            }

            /**
             * Returns a macro that defines how much point lights are expected to be on average in a light
             * grid tile for opaque or transparent geometry.
             *
             * @remark Determines the size for light lists and light grid.
             *
             * @return Macro name and macro value.
             */
            static inline std::pair<std::string, std::string> getAveragePointLightNumPerTileMacro() {
                return {"AVERAGE_POINT_LIGHT_NUM_PER_TILE", "200"};
            }

            /**
             * Returns a macro that defines how much spotlights are expected to be on average in a light grid
             * tile for opaque or transparent geometry.
             *
             * @remark Determines the size for light lists and light grid.
             *
             * @return Macro name and macro value.
             */
            static inline std::pair<std::string, std::string> getAverageSpotLightNumPerTileMacro() {
                return {"AVERAGE_SPOT_LIGHT_NUM_PER_TILE", "200"};
            }
        };
    };
}
