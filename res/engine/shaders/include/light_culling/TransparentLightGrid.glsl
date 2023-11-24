// --------------------------------------------------------------------------------------------------------------------
//                                                light index lists
// --------------------------------------------------------------------------------------------------------------------

/** Stores indices into array of point lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentPointLightIndexList : register(u4, space5);
#glsl{
layout(std430, binding = 13) buffer TransparentPointLightIndexListBuffer{
    uint array[];
} transparentPointLightIndexList;
}

/** Stores indices into array of spot lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentSpotLightIndexList : register(u5, space5);
#glsl{
layout(std430, binding = 14) buffer TransparentSpotLightIndexListBuffer{
    uint array[];
} transparentSpotLightIndexList;
}

/** Stores indices into array of directional lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentDirectionalLightIndexList : register(u6, space5);
#glsl{
layout(std430, binding = 15) buffer TransparentDirectionalLightIndexListBuffer{
    uint array[];
} transparentDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------
//                                                    light grids
// --------------------------------------------------------------------------------------------------------------------

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentPointLightGrid : register(u10, space5);
#glsl layout (binding=19, rg32ui) uniform uimage2D transparentPointLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentSpotLightGrid : register(u11, space5);
#glsl layout (binding=20, rg32ui) uniform uimage2D transparentSpotLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentDirectionalLightGrid : register(u12, space5);
#glsl layout (binding=21, rg32ui) uniform uimage2D transparentDirectionalLightGrid;