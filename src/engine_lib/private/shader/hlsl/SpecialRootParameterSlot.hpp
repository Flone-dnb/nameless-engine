#pragma once

namespace ne{
    /**
     * Constant indices into an array that stores pairs of "slot" - "root parameter index"
     * to query indices of root parameters of various non-user specified resources.
     */
    enum class SpecialRootParameterSlot : unsigned int {
        FRAME_DATA_CBUFFER = 0,
        GENERAL_LIGHTING_CBUFFER,
        POINT_LIGHTS_BUFFER,
        DIRECTIONAL_LIGHTS_BUFFER,
        SPOT_LIGHTS_BUFFER,
        LIGHT_CULLING_POINT_LIGHT_INDEX_LIST,
        LIGHT_CULLING_SPOT_LIGHT_INDEX_LIST,
        LIGHT_CULLING_DIRECTIONAL_LIGHT_INDEX_LIST,
        LIGHT_CULLING_POINT_LIGHT_GRID,
        LIGHT_CULLING_SPOT_LIGHT_GRID,
        LIGHT_CULLING_DIRECTIONAL_LIGHT_GRID,

        // ... new items go here ...

        SIZE, // marks the size of this enum
    };
}
