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

/** Stores some additional information (some information not available as built-in semantics). */
#glsl layout(binding = 2) uniform ThreadGroupCount {
#hlsl struct ThreadGroupCount{
    /** Total number of thread groups dispatched in the X direction. */
    uint iThreadGroupCountX;

    /** Total number of thread groups dispatched in the Y direction. */
    uint iThreadGroupCountY;
#glsl } threadGroupCount;
#hlsl }; ConstantBuffer<ThreadGroupCount> threadGroupCount : register(b0, space5);

// --------------------------------------------------------------------------------------------------------------------
//                                                 counters
// --------------------------------------------------------------------------------------------------------------------

/** A single `uint` value stored at index 0 - global counter into the light list with point lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoOpaquePointLightIndexList : register(u0, space5);
#glsl{
layout(std140, binding = 3) buffer GlobalCounterIntoOpaquePointLightIndexList{
    uint iCounter;
} globalCounterIntoOpaquePointLightIndexList;
}

/** A single `uint` value stored at index 0 - global counter into the light list with spot lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoOpaqueSpotLightIndexList : register(u1, space5);
#glsl{
layout(std140, binding = 4) buffer GlobalCounterIntoOpaqueSpotLightIndexList{
    uint iCounter;
} globalCounterIntoOpaqueSpotLightIndexList;
}

/** A single `uint` value stored at index 0 - global counter into the light list with directional lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoOpaqueDirectionalLightIndexList : register(u2, space5);
#glsl{
layout(std140, binding = 5) buffer GlobalCounterIntoOpaqueDirectionalLightIndexList{
    uint iCounter;
} globalCounterIntoOpaqueDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------                                      

/** A single `uint` value stored at index 0 - global counter into the light list with point lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoTransparentPointLightIndexList : register(u3, space5);
#glsl{
layout(std140, binding = 6) buffer GlobalCounterIntoTransparentPointLightIndexList{
    uint iCounter;
} globalCounterIntoTransparentPointLightIndexList;
}

/** A single `uint` value stored at index 0 - global counter into the light list with spot lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoTransparentSpotLightIndexList : register(u4, space5);
#glsl{
layout(std140, binding = 7) buffer GlobalCounterIntoTransparentSpotLightIndexList{
    uint iCounter;
} globalCounterIntoTransparentSpotLightIndexList;
}

/** A single `uint` value stored at index 0 - global counter into the light list with directional lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> globalCounterIntoTransparentDirectionalLightIndexList : register(u5, space5);
#glsl{
layout(std140, binding = 8) buffer GlobalCounterIntoTransparentDirectionalLightIndexList{
    uint iCounter;
} globalCounterIntoTransparentDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------
//                                                light index lists
// --------------------------------------------------------------------------------------------------------------------

/** Stores indices into array of point lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaquePointLightIndexList : register(u6, space5);
#glsl{
layout(std140, binding = 9) buffer OpaquePointLightIndexListBuffer{
    uint iLightIndex;
} opaquePointLightIndexList;
}

/** Stores indices into array of spot lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaqueSpotLightIndexList : register(u7, space5);
#glsl{
layout(std140, binding = 10) buffer OpaqueSpotLightIndexListBuffer{
    uint iLightIndex;
} opaqueSpotLightIndexList;
}

/** Stores indices into array of directional lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaqueDirectionalLightIndexList : register(u8, space5);
#glsl{
layout(std140, binding = 11) buffer OpaqueDirectionalLightIndexListBuffer{
    uint iLightIndex;
} opaqueDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------    

/** Stores indices into array of point lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentPointLightIndexList : register(u9, space5);
#glsl{
layout(std140, binding = 12) buffer TransparentPointLightIndexListBuffer{
    uint iLightIndex;
} transparentPointLightIndexList;
}

/** Stores indices into array of spot lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentSpotLightIndexList : register(u10, space5);
#glsl{
layout(std140, binding = 13) buffer TransparentSpotLightIndexListBuffer{
    uint iLightIndex;
} transparentSpotLightIndexList;
}

/** Stores indices into array of directional lights for transparent geometry. */
#hlsl RWStructuredBuffer<uint> transparentDirectionalLightIndexList : register(u11, space5);
#glsl{
layout(std140, binding = 14) buffer TransparentDirectionalLightIndexListBuffer{
    uint iLightIndex;
} transparentDirectionalLightIndexList;
}

// --------------------------------------------------------------------------------------------------------------------
//                                                    light grids
// --------------------------------------------------------------------------------------------------------------------

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> opaquePointLightGrid : register(u12, space5);
#glsl layout (binding=15, rg32ui) uniform uimage2D opaquePointLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> opaqueSpotLightGrid : register(u13, space5);
#glsl layout (binding=16, rg32ui) uniform uimage2D opaqueSpotLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> opaqueDirectionalLightGrid : register(u14, space5);
#glsl layout (binding=17, rg32ui) uniform uimage2D opaqueDirectionalLightGrid;

// -------------------------------------------------------------------------------------------------------------------- 

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentPointLightGrid : register(u15, space5);
#glsl layout (binding=18, rg32ui) uniform uimage2D transparentPointLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentSpotLightGrid : register(u16, space5);
#glsl layout (binding=19, rg32ui) uniform uimage2D transparentSpotLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> transparentDirectionalLightGrid : register(u17, space5);
#glsl layout (binding=20, rg32ui) uniform uimage2D transparentDirectionalLightGrid;

// --------------------------------------------------------------------------------------------------------------------
//                                          general group shared (tile) variables
// --------------------------------------------------------------------------------------------------------------------

/** Minimum depth in a light grid tile. Stored as `uint` instead of `float` to allow atomic operations. */
shared uint iTileMinDepth;

/** Maximum depth in a light grid tile. Stored as `uint` instead of `float` to allow atomic operations. */
shared uint iTileMaxDepth;

/** Frustum of a light grid tile. */
shared Frustum tileFrustum;

// --------------------------------------------------------------------------------------------------------------------
//                                          light count - group shared (tile) variables
// --------------------------------------------------------------------------------------------------------------------

/** Stores total number of point lights that intersect frustum of a grid tile for opaque geometry. */
shared uint iOpaquePointLightCountIntersectingTileFrustum;

/** Stores total number of spot lights that intersect frustum of a grid tile for opaque geometry. */
shared uint iOpaqueSpotLightCountIntersectingTileFrustum;

/** Stores total number of directional lights that intersect frustum of a grid tile for opaque geometry. */
shared uint iOpaqueDirectionalLightCountIntersectingTileFrustum;

// -------------------------------------------------------------------------------------------------------------------- 

/** Stores total number of point lights that intersect frustum of a grid tile for transparent geometry. */
shared uint iTransparentPointLightCountIntersectingTileFrustum;

/** Stores total number of spot lights that intersect frustum of a grid tile for transparent geometry. */
shared uint iTransparentSpotLightCountIntersectingTileFrustum;

/** Stores total number of directional lights that intersect frustum of a grid tile for transparent geometry. */
shared uint iTransparentDirectionalLightCountIntersectingTileFrustum;

// --------------------------------------------------------------------------------------------------------------------
//                                 light index list offset - group shared (tile) variables
// --------------------------------------------------------------------------------------------------------------------

/** Offset into the global point light index list for opaque geometry. */
shared uint iOpaquePointLightIndexListStartOffset;

/** Offset into the global spot light index list for opaque geometry. */
shared uint iOpaqueSpotLightIndexListStartOffset;

/** Offset into the global directional light index list for opaque geometry. */
shared uint iOpaqueDirectionalLightIndexListStartOffset;

// -------------------------------------------------------------------------------------------------------------------- 

/** Offset into the global point light index list for transparent geometry. */
shared uint iTransparentPointLightIndexListStartOffset;


/** Offset into the global spot light index list for transparent geometry. */
shared uint iTransparentSpotLightIndexListStartOffset;

/** Offset into the global directional light index list for transparent geometry. */
shared uint iTransparentDirectionalLightIndexListStartOffset;

// --------------------------------------------------------------------------------------------------------------------
//                               local light index list - group shared (tile) variables
// --------------------------------------------------------------------------------------------------------------------

// allowing N lights of a specific type to be in a tile, this limit should never be reached
#define TILE_LIGHT_INDEX_LIST_SIZE AVERAGE_NUM_LIGHTS_OF_SPECIFIC_TYPE_PER_TILE * 5

/** Local light index list that will be copied to the global list index list. */
shared uint tileOpaquePointLightIndexList[TILE_LIGHT_INDEX_LIST_SIZE];

/** Local light index list that will be copied to the global list index list. */
shared uint tileOpaqueSpotLightIndexList[TILE_LIGHT_INDEX_LIST_SIZE];

/** Local light index list that will be copied to the global list index list. */
shared uint tileOpaqueDirectionalLightIndexList[TILE_LIGHT_INDEX_LIST_SIZE];

// --------------------------------------------------------------------------------------------------------------------

/** Local light index list that will be copied to the global list index list. */
shared uint tileTransparentPointLightIndexList[TILE_LIGHT_INDEX_LIST_SIZE];

/** Local light index list that will be copied to the global list index list. */
shared uint tileTransparentSpotLightIndexList[TILE_LIGHT_INDEX_LIST_SIZE];

/** Local light index list that will be copied to the global list index list. */
shared uint tileTransparentDirectionalLightIndexList[TILE_LIGHT_INDEX_LIST_SIZE];

// --------------------------------------------------------------------------------------------------------------------
//                                             general functions
// --------------------------------------------------------------------------------------------------------------------

/**
 * Initialized thread group shared variables.
 *
 * @remark Should be called by a single thread in a thread group.
 *
 * @param iThreadGroupXIdInDispatch X ID of the current group in dispatch (i.e. X ID of the current light grid tile).
 * @param iThreadGroupYIdInDispatch Y ID of the current group in dispatch (i.e. Y ID of the current light grid tile).
 */
void initializeGroupSharedVariables(uint iThreadGroupXIdInDispatch, uint iThreadGroupYIdInDispatch){
    // Initialize depth values.
    iTileMinDepth = 0xffffffff; // uint max
    iTileMaxDepth = 0;

    // Initialize counters.
    iOpaquePointLightCountIntersectingTileFrustum = 0;
    iOpaqueSpotLightCountIntersectingTileFrustum = 0;
    iOpaqueDirectionalLightCountIntersectingTileFrustum = 0;
    iTransparentPointLightCountIntersectingTileFrustum = 0;
    iTransparentSpotLightCountIntersectingTileFrustum = 0;
    iTransparentDirectionalLightCountIntersectingTileFrustum = 0;

    // Initialize offsets.
    iOpaquePointLightIndexListStartOffset = 0;
    iOpaqueSpotLightIndexListStartOffset = 0;
    iOpaqueDirectionalLightIndexListStartOffset = 0;
    iTransparentPointLightIndexListStartOffset = 0;
    iTransparentSpotLightIndexListStartOffset = 0;
    iTransparentDirectionalLightIndexListStartOffset = 0;

    // Prepare index to get tile frustum.
    const uint iFrustumIndex = (iThreadGroupYIdInDispatch * threadGroupCount.iThreadGroupCountX) + iThreadGroupXIdInDispatch;

    // Initialize tile frustum.
    tileFrustum =
#hlsl calculatedFrustums[iFrustumIndex];
#glsl calculatedFrustums.array[iFrustumIndex];
}

// --------------------------------------------------------------------------------------------------------------------
//                                         functions to add lights to tile
// --------------------------------------------------------------------------------------------------------------------

/**
 * Adds an index to the specified point light to array of lights that affect this tile (this thread group)
 * for opaque geometry.
 *
 * @param iPointLightIndex Index to a point light to add.
 */
void addPointLightToTileOpaqueLightIndexList(uint iPointLightIndex){
    // Prepare index to light list.
    uint iIndexToPointLightList = 0;

    // Atomically increment counter of point lights in tile.
#hlsl InterlockedAdd(iOpaquePointLightCountIntersectingTileFrustum, 1, iIndexToPointLightList);
#glsl iIndexToPointLightList = atomicAdd(iOpaquePointLightCountIntersectingTileFrustum, 1);

    // Make sure we won't access out of bound.
    if (iIndexToPointLightList < TILE_LIGHT_INDEX_LIST_SIZE){
        tileOpaquePointLightIndexList[iIndexToPointLightList] = iPointLightIndex;
    }
}

/**
 * Adds an index to the specified spot light to array of lights that affect this tile (this thread group)
 * for opaque geometry.
 *
 * @param iSpotLightIndex Index to a spot light to add.
 */
void addSpotLightToTileOpaqueLightIndexList(uint iSpotLightIndex){
    // Prepare index to light list.
    uint iIndexToSpotLightList = 0;

    // Atomically increment counter of spot lights in tile.
#hlsl InterlockedAdd(iOpaqueSpotLightCountIntersectingTileFrustum, 1, iIndexToSpotLightList);
#glsl iIndexToSpotLightList = atomicAdd(iOpaqueSpotLightCountIntersectingTileFrustum, 1);

    // Make sure we won't access out of bound.
    if (iIndexToSpotLightList < TILE_LIGHT_INDEX_LIST_SIZE){
        tileOpaqueSpotLightIndexList[iIndexToSpotLightList] = iSpotLightIndex;
    }
}

/**
 * Adds an index to the specified directional light to array of lights that affect this tile (this thread group)
 * for opaque geometry.
 *
 * @param iDirectionalLightIndex Index to a directional light to add.
 */
void addDirectionalLightToTileOpaqueLightIndexList(uint iDirectionalLightIndex){
    // Prepare index to light list.
    uint iIndexToDirectionalLightList = 0;

    // Atomically increment counter of directional lights in tile.
#hlsl InterlockedAdd(iOpaqueDirectionalLightCountIntersectingTileFrustum, 1, iIndexToDirectionalLightList);
#glsl iIndexToDirectionalLightList = atomicAdd(iOpaqueDirectionalLightCountIntersectingTileFrustum, 1);

    // Make sure we won't access out of bound.
    if (iIndexToDirectionalLightList < TILE_LIGHT_INDEX_LIST_SIZE){
        tileOpaqueDirectionalLightIndexList[iIndexToDirectionalLightList] = iDirectionalLightIndex;
    }
}

// --------------------------------------------------------------------------------------------------------------------

/**
 * Adds an index to the specified point light to array of lights that affect this tile (this thread group)
 * for transparent geometry.
 *
 * @param iPointLightIndex Index to a point light to add.
 */
void addPointLightToTileTransparentLightIndexList(uint iPointLightIndex){
    // Prepare index to light list.
    uint iIndexToPointLightList = 0;

    // Atomically increment counter of point lights in tile.
#hlsl InterlockedAdd(iTransparentPointLightCountIntersectingTileFrustum, 1, iIndexToPointLightList);
#glsl iIndexToPointLightList = atomicAdd(iTransparentPointLightCountIntersectingTileFrustum, 1);

    // Make sure we won't access out of bound.
    if (iIndexToPointLightList < TILE_LIGHT_INDEX_LIST_SIZE){
        tileTransparentPointLightIndexList[iIndexToPointLightList] = iPointLightIndex;
    }
}

/**
 * Adds an index to the specified spot light to array of lights that affect this tile (this thread group)
 * for transparent geometry.
 *
 * @param iSpotLightIndex Index to a spot light to add.
 */
void addSpotLightToTileTransparentLightIndexList(uint iSpotLightIndex){
    // Prepare index to light list.
    uint iIndexToSpotLightList = 0;

    // Atomically increment counter of spot lights in tile.
#hlsl InterlockedAdd(iTransparentSpotLightCountIntersectingTileFrustum, 1, iIndexToSpotLightList);
#glsl iIndexToSpotLightList = atomicAdd(iTransparentSpotLightCountIntersectingTileFrustum, 1);

    // Make sure we won't access out of bound.
    if (iIndexToSpotLightList < TILE_LIGHT_INDEX_LIST_SIZE){
        tileTransparentSpotLightIndexList[iIndexToSpotLightList] = iSpotLightIndex;
    }
}

/**
 * Adds an index to the specified directional light to array of lights that affect this tile (this thread group)
 * for transparent geometry.
 *
 * @param iDirectionalLightIndex Index to a directional light to add.
 */
void addDirectionalLightToTileTransparentLightIndexList(uint iDirectionalLightIndex){
    // Prepare index to light list.
    uint iIndexToDirectionalLightList = 0;

    // Atomically increment counter of directional lights in tile.
#hlsl InterlockedAdd(iTransparentDirectionalLightCountIntersectingTileFrustum, 1, iIndexToDirectionalLightList);
#glsl iIndexToDirectionalLightList = atomicAdd(iTransparentDirectionalLightCountIntersectingTileFrustum, 1);

    // Make sure we won't access out of bound.
    if (iIndexToDirectionalLightList < TILE_LIGHT_INDEX_LIST_SIZE){
        tileTransparentDirectionalLightIndexList[iIndexToDirectionalLightList] = iDirectionalLightIndex;
    }
}