#include "LightGridMacros.glsl"

// --------------------------------------------------------------------------------------------------------------------
//                                                light index lists
// --------------------------------------------------------------------------------------------------------------------

/** Stores indices into array of point lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaquePointLightIndexList : register(u1, space5);
#glsl{
layout(std430, binding = 10) LIGHT_GRID_QUALIFIER buffer OpaquePointLightIndexListBuffer{
    uint array[];
} opaquePointLightIndexList;
}

/** Stores indices into array of spotlights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaqueSpotLightIndexList : register(u2, space5);
#glsl{
layout(std430, binding = 11) LIGHT_GRID_QUALIFIER buffer OpaqueSpotLightIndexListBuffer{
    uint array[];
} opaqueSpotLightIndexList;
}

/** Stores indices into array of directional lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaqueDirectionalLightIndexList : register(u3, space5);
#glsl{
layout(std430, binding = 12) LIGHT_GRID_QUALIFIER buffer OpaqueDirectionalLightIndexListBuffer{
    uint array[];
} opaqueDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------
//                                                    light grids
// --------------------------------------------------------------------------------------------------------------------

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> opaquePointLightGrid : register(u7, space5);
#glsl layout (binding=16, rg32ui) uniform LIGHT_GRID_QUALIFIER uimage2D opaquePointLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> opaqueSpotLightGrid : register(u8, space5);
#glsl layout (binding=17, rg32ui) uniform LIGHT_GRID_QUALIFIER uimage2D opaqueSpotLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> opaqueDirectionalLightGrid : register(u9, space5);
#glsl layout (binding=18, rg32ui) uniform LIGHT_GRID_QUALIFIER uimage2D opaqueDirectionalLightGrid;