#include "LightGridMacros.glsl"

// --------------------------------------------------------------------------------------------------------------------
//                                                light index lists
// --------------------------------------------------------------------------------------------------------------------

/** Stores indices into array of point lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaquePointLightIndexList : register(u1, space6);
#glsl{
layout(std430, binding = 100) LIGHT_GRID_QUALIFIER buffer OpaquePointLightIndexListBuffer{
    uint array[];
} opaquePointLightIndexList;
}

/** Stores indices into array of spotlights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaqueSpotLightIndexList : register(u2, space6);
#glsl{
layout(std430, binding = 101) LIGHT_GRID_QUALIFIER buffer OpaqueSpotLightIndexListBuffer{
    uint array[];
} opaqueSpotLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------
//                                                    light grids
// --------------------------------------------------------------------------------------------------------------------

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> opaquePointLightGrid : register(u3, space6);
#glsl layout (binding=102, rg32ui) uniform LIGHT_GRID_QUALIFIER uimage2D opaquePointLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> opaqueSpotLightGrid : register(u4, space6);
#glsl layout (binding=103, rg32ui) uniform LIGHT_GRID_QUALIFIER uimage2D opaqueSpotLightGrid;
