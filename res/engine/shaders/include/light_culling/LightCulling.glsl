// This file is used by compute shaders that do Forward+ light culling based.

#include "../Shapes.glsl"
#include "../CoordinateSystemConversion.glsl"

// --------------------------------------------------------------------------------------------------------------------
//                                          general resources
// --------------------------------------------------------------------------------------------------------------------

/** Depth buffer from depth prepass. */
#hlsl Texture2D depthTexture : register(t0, space5);
#glsl layout(binding = 0) uniform sampler2D depthTexture;

/** Calculated grid of frustums in view space. */
#hlsl StructuredBuffer<Frustum> calculatedFrustums : register(t1, space5);
#glsl{
layout(std140, binding = 1) readonly buffer CalculatedFrustumsBuffer{
    Frustum array[];
} calculatedFrustums;
}

// --------------------------------------------------------------------------------------------------------------------
//                                                 counters
// --------------------------------------------------------------------------------------------------------------------

/** A single `uint` value stored at index 0 - global counter into the light list with point lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoOpaquePointLightIndexList : register(u0, space5);
#glsl{
layout(std140, binding = 2) buffer GlobalCounterIntoOpaquePointLightIndexList{
    uint iCounter;
} globalCounterIntoOpaquePointLightIndexList;
}

/** A single `uint` value stored at index 0 - global counter into the light list with spot lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoOpaqueSpotLightIndexList : register(u1, space5);
#glsl{
layout(std140, binding = 3) buffer GlobalCounterIntoOpaqueSpotLightIndexList{
    uint iCounter;
} globalCounterIntoOpaqueSpotLightIndexList;
}

/** A single `uint` value stored at index 0 - global counter into the light list with directional lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoOpaqueDirectionalLightIndexList : register(u2, space5);
#glsl{
layout(std140, binding = 4) buffer GlobalCounterIntoOpaqueDirectionalLightIndexList{
    uint iCounter;
} globalCounterIntoOpaqueDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------                                      

/** A single `uint` value stored at index 0 - global counter into the light list with point lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoTransparentPointLightIndexList : register(u3, space5);
#glsl{
layout(std140, binding = 5) buffer GlobalCounterIntoTransparentPointLightIndexList{
    uint iCounter;
} globalCounterIntoTransparentPointLightIndexList;
}

/** A single `uint` value stored at index 0 - global counter into the light list with spot lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoTransparentSpotLightIndexList : register(u4, space5);
#glsl{
layout(std140, binding = 6) buffer GlobalCounterIntoTransparentSpotLightIndexList{
    uint iCounter;
} globalCounterIntoTransparentSpotLightIndexList;
}

/** A single `uint` value stored at index 0 - global counter into the light list with directional lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoTransparentDirectionalLightIndexList : register(u5, space5);
#glsl{
layout(std140, binding = 7) buffer GlobalCounterIntoTransparentDirectionalLightIndexList{
    uint iCounter;
} globalCounterIntoTransparentDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------
//                                                light index lists
// --------------------------------------------------------------------------------------------------------------------

/** Stores indices into array of point lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaquePointLightIndexList : register(u6, space5);
#glsl{
layout(std140, binding = 8) buffer OpaquePointLightIndexListBuffer{
    uint iLightIndex;
} opaquePointLightIndexList;
}

/** Stores indices into array of spot lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaqueSpotLightIndexList : register(u7, space5);
#glsl{
layout(std140, binding = 9) buffer OpaqueSpotLightIndexListBuffer{
    uint iLightIndex;
} opaqueSpotLightIndexList;
}

/** Stores indices into array of directional lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaqueDirectionalLightIndexList : register(u8, space5);
#glsl{
layout(std140, binding = 10) buffer OpaqueDirectionalLightIndexListBuffer{
    uint iLightIndex;
} opaqueDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------    

/** Stores indices into array of point lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentPointLightIndexList : register(u9, space5);
#glsl{
layout(std140, binding = 11) buffer TransparentPointLightIndexListBuffer{
    uint iLightIndex;
} transparentPointLightIndexList;
}

/** Stores indices into array of spot lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentSpotLightIndexList : register(u10, space5);
#glsl{
layout(std140, binding = 12) buffer TransparentSpotLightIndexListBuffer{
    uint iLightIndex;
} transparentSpotLightIndexList;
}

/** Stores indices into array of directional lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentDirectionalLightIndexList : register(u11, space5);
#glsl{
layout(std140, binding = 13) buffer TransparentDirectionalLightIndexListBuffer{
    uint iLightIndex;
} transparentDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------
//                                                    light grids
// --------------------------------------------------------------------------------------------------------------------

// TODO