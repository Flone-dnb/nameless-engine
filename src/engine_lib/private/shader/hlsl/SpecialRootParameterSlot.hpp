#pragma once

namespace ne {
    /**
     * Constant indices into an array that stores pairs of "slot" - "root parameter index"
     * to query indices of root parameters of various non-user specified resources.
     */
    enum class SpecialRootParameterSlot : unsigned int {
        GENERAL_LIGHTING = 0,
        POINT_LIGHTS,
        DIRECTIONAL_LIGHTS,
        SPOT_LIGHTS,
        LIGHT_CULLING_POINT_LIGHT_INDEX_LIST,
        LIGHT_CULLING_SPOT_LIGHT_INDEX_LIST,
        LIGHT_CULLING_POINT_LIGHT_GRID,
        LIGHT_CULLING_SPOT_LIGHT_GRID,
        SHADOW_PASS_LIGHT_INFO,
        ROOT_CONSTANTS,
        DIRECTIONAL_SHADOW_MAPS,
        SPOT_SHADOW_MAPS,
        POINT_SHADOW_MAPS,

        // ... new items go here ...

        SIZE, // marks the size of this enum
    };
}
