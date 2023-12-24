#include "LightGridMacros.glsl"

// --------------------------------------------------------------------------------------------------------------------
//                                                light index lists
// --------------------------------------------------------------------------------------------------------------------

/** Stores indices into array of point lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentPointLightIndexList : register(u4, space6);
#glsl{
layout(std430, binding = 106) LIGHT_GRID_QUALIFIER buffer TransparentPointLightIndexListBuffer{
    uint array[];
} transparentPointLightIndexList;
}

/** Stores indices into array of spot lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentSpotLightIndexList : register(u5, space6);
#glsl{
layout(std430, binding = 107) LIGHT_GRID_QUALIFIER buffer TransparentSpotLightIndexListBuffer{
    uint array[];
} transparentSpotLightIndexList;
}

/** Stores indices into array of directional lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentDirectionalLightIndexList : register(u6, space6);
#glsl{
layout(std430, binding = 108) LIGHT_GRID_QUALIFIER buffer TransparentDirectionalLightIndexListBuffer{
    uint array[];
} transparentDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------
//                                                    light grids
// --------------------------------------------------------------------------------------------------------------------

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentPointLightGrid : register(u10, space6);
#glsl layout (binding=109, rg32ui) uniform LIGHT_GRID_QUALIFIER uimage2D transparentPointLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentSpotLightGrid : register(u11, space6);
#glsl layout (binding=110, rg32ui) uniform LIGHT_GRID_QUALIFIER uimage2D transparentSpotLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentDirectionalLightGrid : register(u12, space6);
#glsl layout (binding=111, rg32ui) uniform LIGHT_GRID_QUALIFIER uimage2D transparentDirectionalLightGrid;