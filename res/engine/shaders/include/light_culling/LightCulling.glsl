// This file is used by compute shaders that do Forward+ light culling based.

#include "../Shapes.glsl"
#include "../CoordinateSystemConversion.glsl"
#include "../FrameData.glsl"
#include "../Lighting.glsl"

// --------------------------------------------------------------------------------------------------------------------
//                                          general resources
// --------------------------------------------------------------------------------------------------------------------

/** Depth buffer from depth prepass. */
#hlsl Texture2D depthTexture : register(t3, space5);
#glsl layout(binding = 5) uniform sampler2D depthTexture;

/** Calculated grid of frustums in view space. */
#hlsl StructuredBuffer<Frustum> calculatedFrustums : register(t4, space5);
#glsl{
layout(std140, binding = 6) readonly buffer CalculatedFrustumsBuffer{
    Frustum array[];
} calculatedFrustums;
}

/** Stores some additional information (some information not available as built-in semantics). */
#glsl layout(binding = 7) uniform ThreadGroupCount {
#hlsl struct ThreadGroupCount{
    /** Total number of thread groups dispatched in the X direction. */
    uint iThreadGroupCountX;

    /** Total number of thread groups dispatched in the Y direction. */
    uint iThreadGroupCountY;
#glsl } threadGroupCount;
#hlsl }; ConstantBuffer<ThreadGroupCount> threadGroupCount : register(b2, space5);

/** Data that we need to convert coordinates from screen space to view space. */
#glsl layout(binding = 8) uniform ScreenToViewData {
#hlsl struct ScreenToViewData{
    /** Inverse of the projection matrix. */
    mat4 inverseProjectionMatrix;

    /** Width of the viewport (might be smaller that the actual screen size). */
    uint iRenderResolutionWidth;

    /** Height of the viewport (might be smaller that the actual screen size). */
    uint iRenderResolutionHeight;
#glsl } screenToViewData;
#hlsl }; ConstantBuffer<ScreenToViewData> screenToViewData : register(b3, space5);

// --------------------------------------------------------------------------------------------------------------------
//                                                 counters
// --------------------------------------------------------------------------------------------------------------------

#include "CountersData.glsl"

// --------------------------------------------------------------------------------------------------------------------
//                                                light index lists
// --------------------------------------------------------------------------------------------------------------------

/** Stores indices into array of point lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaquePointLightIndexList : register(u1, space5);
#glsl{
layout(std430, binding = 10) buffer OpaquePointLightIndexListBuffer{
    uint array[];
} opaquePointLightIndexList;
}

/** Stores indices into array of spotlights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaqueSpotLightIndexList : register(u2, space5);
#glsl{
layout(std430, binding = 11) buffer OpaqueSpotLightIndexListBuffer{
    uint array[];
} opaqueSpotLightIndexList;
}

/** Stores indices into array of directional lights for opaque geometry. */
#hlsl RWStructuredBuffer<uint> opaqueDirectionalLightIndexList : register(u3, space5);
#glsl{
layout(std430, binding = 12) buffer OpaqueDirectionalLightIndexListBuffer{
    uint array[];
} opaqueDirectionalLightIndexList;
}

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
#hlsl RWTexture2D<uint2> opaquePointLightGrid : register(u7, space5);
#glsl layout (binding=16, rg32ui) uniform uimage2D opaquePointLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> opaqueSpotLightGrid : register(u8, space5);
#glsl layout (binding=17, rg32ui) uniform uimage2D opaqueSpotLightGrid;

/** Light grid where every pixel stores 2 values: offset into light index list and the number of elements to read from that offset. */
#hlsl RWTexture2D<uint2> opaqueDirectionalLightGrid : register(u9, space5);
#glsl layout (binding=18, rg32ui) uniform uimage2D opaqueDirectionalLightGrid;

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
#define TILE_POINT_LIGHT_INDEX_LIST_SIZE        AVERAGE_POINT_LIGHT_NUM_PER_TILE       * 5
#define TILE_SPOT_LIGHT_INDEX_LIST_SIZE         AVERAGE_SPOT_LIGHT_NUM_PER_TILE        * 5
#define TILE_DIRECTIONAL_LIGHT_INDEX_LIST_SIZE  AVERAGE_DIRECTIONAL_LIGHT_NUM_PER_TILE * 5

/** Local light index list that will be copied to the global list index list. */
shared uint tileOpaquePointLightIndexList[TILE_POINT_LIGHT_INDEX_LIST_SIZE];

/** Local light index list that will be copied to the global list index list. */
shared uint tileOpaqueSpotLightIndexList[TILE_SPOT_LIGHT_INDEX_LIST_SIZE];

/** Local light index list that will be copied to the global list index list. */
shared uint tileOpaqueDirectionalLightIndexList[TILE_DIRECTIONAL_LIGHT_INDEX_LIST_SIZE];

// --------------------------------------------------------------------------------------------------------------------

/** Local light index list that will be copied to the global list index list. */
shared uint tileTransparentPointLightIndexList[TILE_POINT_LIGHT_INDEX_LIST_SIZE];

/** Local light index list that will be copied to the global list index list. */
shared uint tileTransparentSpotLightIndexList[TILE_SPOT_LIGHT_INDEX_LIST_SIZE];

/** Local light index list that will be copied to the global list index list. */
shared uint tileTransparentDirectionalLightIndexList[TILE_DIRECTIONAL_LIGHT_INDEX_LIST_SIZE];

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
    iTileMinDepth = 0xffffffffu; // uint max
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

/**
 * Performs atomic min/max operation on the specified depth value to calculate tile's min and max depth.
 * Also waits for all threads from group to perform atomic min/max operations so that after the function
 * is finished the tile's min/max depth is finally calculated.
 *
 * @warning Expects the depth value to be in range [0.0; 1.0].
 *
 * @param pixelDepth Depth in range [0.0; 1.0] of a pixel (thread) to count for tile's min/max depth.
 */
void calculateTileMinMaxDepth(float pixelDepth){
    // Reinterpret float as uint so that we will be able to do atomic operations on it.
    // Since our depth is in range [0..1] (we don't have negative depth values) doing `asuint` is enough
    // and we don't need to change float sign bit and we don't need to invert negative bits and etc.
    uint iDepth =
#hlsl asuint(pixelDepth);
#glsl floatBitsToUint(pixelDepth);

    // Do atomic min on depth.
#hlsl InterlockedMin(iTileMinDepth, iDepth);
#glsl atomicMin(iTileMinDepth, iDepth);

    // Do atomic min on depth.
#hlsl InterlockedMax(iTileMaxDepth, iDepth);
#glsl atomicMax(iTileMaxDepth, iDepth);

    // Wait for all threads from group.
#hlsl GroupMemoryBarrierWithGroupSync();
#glsl groupMemoryBarrier(); barrier();
}

/**
 * "Reserves" space for thread group in light grid and light lists and writes information about non-culled
 * lights of a tile into the light grid.
 *
 * @remark Should be called by a single thread in a thread group.
 *
 * @param iThreadGroupXIdInDispatch X ID of the current group in dispatch (i.e. X ID of the current light grid tile).
 * @param iThreadGroupYIdInDispatch Y ID of the current group in dispatch (i.e. Y ID of the current light grid tile).
 */
void reserveSpaceForNonCulledTileLightsInLightGridAndList(uint iThreadGroupXIdInDispatch, uint iThreadGroupYIdInDispatch){
    // Prepare some constants.
#hlsl const uint2 lightGridIndex = uint2(iThreadGroupXIdInDispatch, iThreadGroupYIdInDispatch);
#glsl const ivec2 lightGridIndex = ivec2(iThreadGroupXIdInDispatch, iThreadGroupYIdInDispatch);

    // Start with opaque geometry.

    // Get offset into the global light index list with point lights.
#hlsl InterlockedAdd(globalCountersIntoLightIndexList[0].iPointLightListOpaque, iOpaquePointLightCountIntersectingTileFrustum, iOpaquePointLightIndexListStartOffset);
#glsl iOpaquePointLightIndexListStartOffset = atomicAdd(globalCountersIntoLightIndexList.iPointLightListOpaque, iOpaquePointLightCountIntersectingTileFrustum);

    // Write lights into the light grid.
#hlsl opaquePointLightGrid[lightGridIndex] = uint2(iOpaquePointLightIndexListStartOffset, iOpaquePointLightCountIntersectingTileFrustum);
#glsl imageStore(opaquePointLightGrid, lightGridIndex, uvec4(iOpaquePointLightIndexListStartOffset, iOpaquePointLightCountIntersectingTileFrustum, 0.0F, 0.0F));
    
    // Get offset into the global light index list with spot lights.
#hlsl InterlockedAdd(globalCountersIntoLightIndexList[0].iSpotlightListOpaque, iOpaqueSpotLightCountIntersectingTileFrustum, iOpaqueSpotLightIndexListStartOffset);
#glsl iOpaqueSpotLightIndexListStartOffset = atomicAdd(globalCountersIntoLightIndexList.iSpotlightListOpaque, iOpaqueSpotLightCountIntersectingTileFrustum);

    // Write lights into the light grid.
#hlsl opaqueSpotLightGrid[lightGridIndex] = uint2(iOpaqueSpotLightIndexListStartOffset, iOpaqueSpotLightCountIntersectingTileFrustum);
#glsl imageStore(opaqueSpotLightGrid, lightGridIndex, uvec4(iOpaqueSpotLightIndexListStartOffset, iOpaqueSpotLightCountIntersectingTileFrustum, 0.0F, 0.0F));

    // Get offset into the global light index list with directional lights.
#hlsl InterlockedAdd(globalCountersIntoLightIndexList[0].iDirectionalLightListOpaque, iOpaqueDirectionalLightCountIntersectingTileFrustum, iOpaqueDirectionalLightIndexListStartOffset);
#glsl iOpaqueDirectionalLightIndexListStartOffset = atomicAdd(globalCountersIntoLightIndexList.iDirectionalLightListOpaque, iOpaqueDirectionalLightCountIntersectingTileFrustum);

    // Write lights into the light grid.
#hlsl opaqueDirectionalLightGrid[lightGridIndex] = uint2(iOpaqueDirectionalLightIndexListStartOffset, iOpaqueDirectionalLightCountIntersectingTileFrustum);
#glsl imageStore(opaqueDirectionalLightGrid, lightGridIndex, uvec4(iOpaqueDirectionalLightIndexListStartOffset, iOpaqueDirectionalLightCountIntersectingTileFrustum, 0.0F, 0.0F));

    // Now set lights for transparent geometry.

    // Get offset into the global light index list with point lights.
#hlsl InterlockedAdd(globalCountersIntoLightIndexList[0].iPointLightListTransparent, iTransparentPointLightCountIntersectingTileFrustum, iTransparentPointLightIndexListStartOffset);
#glsl iTransparentPointLightIndexListStartOffset = atomicAdd(globalCountersIntoLightIndexList.iPointLightListTransparent, iTransparentPointLightCountIntersectingTileFrustum);

    // Write lights into the light grid.
#hlsl transparentPointLightGrid[lightGridIndex] = uint2(iTransparentPointLightIndexListStartOffset, iTransparentPointLightCountIntersectingTileFrustum);
#glsl imageStore(transparentPointLightGrid, lightGridIndex, uvec4(iTransparentPointLightIndexListStartOffset, iTransparentPointLightCountIntersectingTileFrustum, 0.0F, 0.0F));
    
    // Get offset into the global light index list with spot lights.
#hlsl InterlockedAdd(globalCountersIntoLightIndexList[0].iSpotlightListTransparent, iTransparentSpotLightCountIntersectingTileFrustum, iTransparentSpotLightIndexListStartOffset);
#glsl iTransparentSpotLightIndexListStartOffset = atomicAdd(globalCountersIntoLightIndexList.iSpotlightListTransparent, iTransparentSpotLightCountIntersectingTileFrustum);

    // Write lights into the light grid.
#hlsl transparentSpotLightGrid[lightGridIndex] = uint2(iTransparentSpotLightIndexListStartOffset, iTransparentSpotLightCountIntersectingTileFrustum);
#glsl imageStore(transparentSpotLightGrid, lightGridIndex, uvec4(iTransparentSpotLightIndexListStartOffset, iTransparentSpotLightCountIntersectingTileFrustum, 0.0F, 0.0F));

    // Get offset into the global light index list with directional lights.
#hlsl InterlockedAdd(globalCountersIntoLightIndexList[0].iDirectionalLightListTransparent, iTransparentDirectionalLightCountIntersectingTileFrustum, iTransparentDirectionalLightIndexListStartOffset);
#glsl iTransparentDirectionalLightIndexListStartOffset = atomicAdd(globalCountersIntoLightIndexList.iDirectionalLightListTransparent, iTransparentDirectionalLightCountIntersectingTileFrustum);

    // Write lights into the light grid.
#hlsl transparentDirectionalLightGrid[lightGridIndex] = uint2(iTransparentDirectionalLightIndexListStartOffset, iTransparentDirectionalLightCountIntersectingTileFrustum);
#glsl imageStore(transparentDirectionalLightGrid, lightGridIndex, uvec4(iTransparentDirectionalLightIndexListStartOffset, iTransparentDirectionalLightCountIntersectingTileFrustum, 0.0F, 0.0F));
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
    if (iIndexToPointLightList < TILE_POINT_LIGHT_INDEX_LIST_SIZE){
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
    if (iIndexToSpotLightList < TILE_SPOT_LIGHT_INDEX_LIST_SIZE){
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
    if (iIndexToDirectionalLightList < TILE_DIRECTIONAL_LIGHT_INDEX_LIST_SIZE){
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
    if (iIndexToPointLightList < TILE_POINT_LIGHT_INDEX_LIST_SIZE){
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
    if (iIndexToSpotLightList < TILE_SPOT_LIGHT_INDEX_LIST_SIZE){
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
    if (iIndexToDirectionalLightList < TILE_DIRECTIONAL_LIGHT_INDEX_LIST_SIZE){
        tileTransparentDirectionalLightIndexList[iIndexToDirectionalLightList] = iDirectionalLightIndex;
    }
}

// --------------------------------------------------------------------------------------------------------------------
//                                              light culling functions
// --------------------------------------------------------------------------------------------------------------------

void cullLightsForTile(float pixelDepth, uint iThreadGroupXIdInDispatch, uint iThreadGroupYIdInDispatch, uint iThreadIdInGroup){
    // Sources:
    // - Presentation "DirectX 11 Rendering in Battlefield 3" (2011) by Johan Andersson, DICE.
    // - "Forward+: A Step Toward Film-Style Shading in Real Time", Takahiro Harada (2012).

    if (iThreadIdInGroup == 0){
        // Only one thread in the group should initialize group shared variables.
        initializeGroupSharedVariables(iThreadGroupXIdInDispatch, iThreadGroupYIdInDispatch);
    }

    // Make sure all group shared writes were finished and all threads from the group reached this line.
#hlsl GroupMemoryBarrierWithGroupSync();
#glsl groupMemoryBarrier(); barrier();

    // Calculate min/max depth for tile.
    calculateTileMinMaxDepth(pixelDepth); // waits inside of the function

    // Get tile min depth.
    float tileMinDepth =
#hlsl asfloat(iTileMinDepth);
#glsl uintBitsToFloat(iTileMinDepth);

    // Get tile max depth.
    float tileMaxDepth = 
#hlsl asfloat(iTileMaxDepth);
#glsl uintBitsToFloat(iTileMaxDepth);

    // Convert depth values to view space.
    float tileMinDepthViewSpace
        = convertNdcSpaceToViewSpace(vec3(0.0F, 0.0F, tileMinDepth), screenToViewData.inverseProjectionMatrix).z;
    float tileMaxDepthViewSpace
        = convertNdcSpaceToViewSpace(vec3(0.0F, 0.0F, tileMaxDepth), screenToViewData.inverseProjectionMatrix).z;

    // Calculate near clip plane distance for culling lights for transparent geometry.
    float nearClipPlaneViewSpace
        = convertNdcSpaceToViewSpace(vec3(0.0F, 0.0F, 0.0F), screenToViewData.inverseProjectionMatrix).z;

    // Prepare a plane to cull lights for opaque geometry.
    Plane minDepthPlaneViewSpace;
    minDepthPlaneViewSpace.normal = vec3(0.0F, 0.0F, 1.0F);
    minDepthPlaneViewSpace.distanceFromOrigin = tileMinDepthViewSpace;

    // Each iteration will now process `THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY` lights in parallel for the tile:
    // thread 0 processes lights: 0, THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY, (THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY) * 2, etc.
    // thread 1 processes lights: 1, THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY + 1, etc.

    // Cull point lights.
    for (uint i = iThreadIdInGroup; i < generalLightingData.iPointLightCount; i += THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY){
        // Get point light.
        PointLight pointLight =
#hlsl         pointLights[i];
#glsl         pointLights.array[i];

        // Calculate light view space position.
        vec3 lightViewSpacePosition = 
#hlsl mul(frameData.viewMatrix, float4(pointLight.position.xyz, 1.0F)).xyz;
#glsl (frameData.viewMatrix * vec4(pointLight.position.xyz, 1.0F)).xyz;

        // Construct a sphere according to point light's radius for sphere/frustum test.
        Sphere pointLightSphereViewSpace;
        pointLightSphereViewSpace.center = lightViewSpacePosition;
        pointLightSphereViewSpace.radius = pointLight.distance;

        // First test for transparent geometry since frustum for transparent objects has more Z space.
        if (!isSphereInsideFrustum(pointLightSphereViewSpace, tileFrustum, nearClipPlaneViewSpace, tileMaxDepthViewSpace)){
            continue;
        }

        // Append this light to transparent geometry.
        addPointLightToTileTransparentLightIndexList(i);

        // Now we know that the light is inside frustum for transparent objects, frustum for opaque objects is
        // the same expect it has smaller Z range so it's enough to just test sphere/plane.
        if (isSphereBehindPlane(pointLightSphereViewSpace, minDepthPlaneViewSpace)){
            continue;
        }

        // Append this light to opaque geometry.
        addPointLightToTileOpaqueLightIndexList(i);
    }

    // Cull spot lights.
    for (uint i = iThreadIdInGroup; i < generalLightingData.iSpotlightCount; i += THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY){
        // Get spot light.
        Spotlight spotlight =
#hlsl         spotlights[i];
#glsl         spotlights.array[i];

        // Calculate light view space position.
        vec3 lightViewSpacePosition = 
#hlsl mul(frameData.viewMatrix, float4(spotlight.position.xyz, 1.0F)).xyz;
#glsl (frameData.viewMatrix * vec4(spotlight.position.xyz, 1.0F)).xyz;

        // Calculate light view space direction.
        vec3 lightViewSpaceDirection = 
#hlsl mul(frameData.viewMatrix, float4(spotlight.direction.xyz, 1.0F)).xyz;
#glsl (frameData.viewMatrix * vec4(spotlight.direction.xyz, 1.0F)).xyz;

        // Construct a cone according to spotlight data for cone/frustum test.
        Cone spotlightConeViewSpace;
        spotlightConeViewSpace.location = lightViewSpacePosition;
        spotlightConeViewSpace.height = spotlight.distance;
        spotlightConeViewSpace.direction = lightViewSpaceDirection;
        spotlightConeViewSpace.bottomRadius = spotlight.coneBottomRadius;

        // First test for transparent geometry since frustum for transparent objects has more Z space.
        if (!isConeInsideFrustum(spotlightConeViewSpace, tileFrustum, nearClipPlaneViewSpace, tileMaxDepthViewSpace)){
            continue;
        }
  
        // Append this light to transparent geometry.
        addSpotLightToTileTransparentLightIndexList(i);

        // Now we know that the light is inside frustum for transparent objects, frustum for opaque objects is
        // the same expect it has smaller Z range so it's enough to just test cone/plane.
        if (isConeBehindPlane(spotlightConeViewSpace, minDepthPlaneViewSpace)){
            continue;
        }

        // Append this light to opaque geometry.
        addSpotLightToTileOpaqueLightIndexList(i);
    }

    // Cull directional lights.
    for (uint i = iThreadIdInGroup; i < generalLightingData.iDirectionalLightCount; i += THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY){
        // Since directional lights have infinite light distance just add them.
        
        // Append this light to transparent geometry.
        addDirectionalLightToTileTransparentLightIndexList(i);

        // Append this light to opaque geometry.
        addDirectionalLightToTileOpaqueLightIndexList(i);
    }

    // Wait for all group threads to cull lights and finish writing to groupshared memory.
#hlsl GroupMemoryBarrierWithGroupSync();
#glsl groupMemoryBarrier(); barrier();

    if (iThreadIdInGroup == 0){
        // Write non-culled lights to light grid and reserve space in global light list.
        reserveSpaceForNonCulledTileLightsInLightGridAndList(iThreadGroupXIdInDispatch, iThreadGroupYIdInDispatch);
    }

    // Wait for all group threads before updating global light index list.
#hlsl GroupMemoryBarrierWithGroupSync();
#glsl groupMemoryBarrier(); barrier();

    // Write local light index list into the global light index list.
    // Start with opaque geometry.

    // Write point lights.
    for (uint i = iThreadIdInGroup; i < iOpaquePointLightCountIntersectingTileFrustum; i += THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY){
#hlsl    opaquePointLightIndexList
#glsl    opaquePointLightIndexList.array
            [iOpaquePointLightIndexListStartOffset + i] = tileOpaquePointLightIndexList[i];
    }

    // Write spot lights.
    for (uint i = iThreadIdInGroup; i < iOpaqueSpotLightCountIntersectingTileFrustum; i += THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY){
#hlsl    opaqueSpotLightIndexList
#glsl    opaqueSpotLightIndexList.array
            [iOpaqueSpotLightIndexListStartOffset + i] = tileOpaqueSpotLightIndexList[i];
    }

    // Write directional lights.
    for (uint i = iThreadIdInGroup; i < iOpaqueDirectionalLightCountIntersectingTileFrustum; i += THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY){
#hlsl    opaqueDirectionalLightIndexList
#glsl    opaqueDirectionalLightIndexList.array
            [iOpaqueDirectionalLightIndexListStartOffset + i] = tileOpaqueDirectionalLightIndexList[i];
    }

    // Now write lights for transparent geometry.

    // Write point lights.
    for (uint i = iThreadIdInGroup; i < iTransparentPointLightCountIntersectingTileFrustum; i += THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY){
#hlsl    transparentPointLightIndexList
#glsl    transparentPointLightIndexList.array
            [iTransparentPointLightIndexListStartOffset + i] = tileTransparentPointLightIndexList[i];
    }

    // Write spot lights.
    for (uint i = iThreadIdInGroup; i < iTransparentSpotLightCountIntersectingTileFrustum; i += THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY){
#hlsl    transparentSpotLightIndexList
#glsl    transparentSpotLightIndexList.array
            [iTransparentSpotLightIndexListStartOffset + i] = tileTransparentSpotLightIndexList[i];
    }

    // Write directional lights.
    for (uint i = iThreadIdInGroup; i < iTransparentDirectionalLightCountIntersectingTileFrustum; i += THREADS_IN_GROUP_XY * THREADS_IN_GROUP_XY){
#hlsl    transparentDirectionalLightIndexList
#glsl    transparentDirectionalLightIndexList.array
            [iTransparentDirectionalLightIndexListStartOffset + i] = tileTransparentDirectionalLightIndexList[i];
    }
}