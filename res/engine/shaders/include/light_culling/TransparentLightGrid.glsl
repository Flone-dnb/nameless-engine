#include "LightGridMacros.glsl"

// --------------------------------------------------------------------------------------------------------------------
//                                                light index lists
// --------------------------------------------------------------------------------------------------------------------

/** Stores indices into array of point lights (that array is used during the main pass) for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentPointLightIndexList : register(u5, space6);
#glsl {
    layout(std430, binding = 104) LIGHT_GRID_QUALIFIER buffer TransparentPointLightIndexListBuffer {
        /** Indices into array. */
        uint array[];
    } transparentPointLightIndexList;
}

/** Stores indices into array of spot lights (that array is used during the main pass) for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentSpotLightIndexList : register(u6, space6);
#glsl {
    layout(std430, binding = 105) LIGHT_GRID_QUALIFIER buffer TransparentSpotLightIndexListBuffer {
        /** Indices into array. */
        uint array[];
    } transparentSpotLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------
//                                                    light grids
// --------------------------------------------------------------------------------------------------------------------

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentPointLightGrid : register(u7, space6);
#glsl layout (binding=106, rg32ui) uniform LIGHT_GRID_QUALIFIER uimage2D transparentPointLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentSpotLightGrid : register(u8, space6);
#glsl layout (binding=107, rg32ui) uniform LIGHT_GRID_QUALIFIER uimage2D transparentSpotLightGrid;
