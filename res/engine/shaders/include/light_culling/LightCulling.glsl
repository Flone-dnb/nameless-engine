// This file is used by compute shaders that do Forward+ light culling based.

#include "../Shapes.glsl"
#include "../CoordinateSystemConversion.glsl"
#include "../FrameData.glsl"
#include "../Lighting.glsl"
#include "CountersData.glsl"
#include "OpaqueLightGrid.glsl"

// --------------------------------------------------------------------------------------------------------------------
//                                          general resources
// --------------------------------------------------------------------------------------------------------------------

/** Depth buffer from depth prepass. */
#hlsl Texture2D depthTexture : register(t0, space6);
#glsl layout(binding = 7) uniform sampler2D depthTexture;

/** Calculated grid of frustums in view space. */
#hlsl StructuredBuffer<Frustum> calculatedFrustums : register(t1, space6);
#glsl {
    layout(std140, binding = 8) readonly buffer CalculatedFrustumsBuffer {
        Frustum array[];
    } calculatedFrustums;
}

/** Stores some additional information (some information not available as built-in semantics). */
#glsl layout(binding = 9) uniform ThreadGroupCount {
#hlsl struct ThreadGroupCount {
        /** Total number of thread groups dispatched in the X direction. */
        uint iThreadGroupCountX;
        
        /** Total number of thread groups dispatched in the Y direction. */
        uint iThreadGroupCountY;
#glsl } threadGroupCount;
#hlsl }; ConstantBuffer<ThreadGroupCount> threadGroupCount : register(b0, space6);

/** Data that we need to convert coordinates from screen space to view space. */
#glsl layout(binding = 10) uniform ScreenToViewData {
#hlsl struct ScreenToViewData {
        /** Inverse of the projection matrix. */
        mat4 inverseProjectionMatrix;
        
        /** Width of the viewport (might be smaller that the actual screen size). */
        uint iRenderTargetWidth;
        
        /** Height of the viewport (might be smaller that the actual screen size). */
        uint iRenderTargetHeight;
#glsl } screenToViewData;
#hlsl }; ConstantBuffer<ScreenToViewData> screenToViewData : register(b1, space6);

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

// --------------------------------------------------------------------------------------------------------------------
//                                 light index list offset - group shared (tile) variables
// --------------------------------------------------------------------------------------------------------------------

/** Offset into the global point light index list for opaque geometry. */
shared uint iOpaquePointLightIndexListStartOffset;

/** Offset into the global spot light index list for opaque geometry. */
shared uint iOpaqueSpotLightIndexListStartOffset;

// --------------------------------------------------------------------------------------------------------------------
//                               local light index list - group shared (tile) variables
// --------------------------------------------------------------------------------------------------------------------

// allowing N lights of a specific type to be in a tile, this limit should never be reached
#define TILE_POINT_LIGHT_INDEX_LIST_SIZE AVERAGE_POINT_LIGHT_NUM_PER_TILE * 5
#define TILE_SPOT_LIGHT_INDEX_LIST_SIZE  AVERAGE_SPOT_LIGHT_NUM_PER_TILE  * 5

/** Local light index list that will be copied to the global list index list. */
shared uint tileOpaquePointLightIndexList[TILE_POINT_LIGHT_INDEX_LIST_SIZE];

/** Local light index list that will be copied to the global list index list. */
shared uint tileOpaqueSpotLightIndexList[TILE_SPOT_LIGHT_INDEX_LIST_SIZE];

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
void initializeGroupSharedVariables(uint iThreadGroupXIdInDispatch, uint iThreadGroupYIdInDispatch) {
    // Initialize depth values.
    iTileMinDepth = 0xffffffffu; // uint max
    iTileMaxDepth = 0;
    
    // Initialize counters.
    iOpaquePointLightCountIntersectingTileFrustum = 0;
    iOpaqueSpotLightCountIntersectingTileFrustum = 0;
    
    // Initialize offsets.
    iOpaquePointLightIndexListStartOffset = 0;
    iOpaqueSpotLightIndexListStartOffset = 0;
    
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
void calculateTileMinMaxDepth(float pixelDepth) {
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
void reserveSpaceForNonCulledTileLightsInLightGridAndList(uint iThreadGroupXIdInDispatch, uint iThreadGroupYIdInDispatch) {
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
void addPointLightToTileOpaqueLightIndexList(uint iPointLightIndex) {
    // Prepare index to light list.
    uint iIndexToPointLightList = 0;
    
    // Atomically increment counter of point lights in tile.
#hlsl InterlockedAdd(iOpaquePointLightCountIntersectingTileFrustum, 1, iIndexToPointLightList);
#glsl iIndexToPointLightList = atomicAdd(iOpaquePointLightCountIntersectingTileFrustum, 1);
    
    // Make sure we won't access out of bound.
    if (iIndexToPointLightList < TILE_POINT_LIGHT_INDEX_LIST_SIZE) {
        tileOpaquePointLightIndexList[iIndexToPointLightList] = iPointLightIndex;
    }
}

/**
 * Adds an index to the specified spot light to array of lights that affect this tile (this thread group)
 * for opaque geometry.
 *
 * @param iSpotLightIndex Index to a spot light to add.
 */
void addSpotLightToTileOpaqueLightIndexList(uint iSpotLightIndex) {
    // Prepare index to light list.
    uint iIndexToSpotLightList = 0;
    
    // Atomically increment counter of spot lights in tile.
#hlsl InterlockedAdd(iOpaqueSpotLightCountIntersectingTileFrustum, 1, iIndexToSpotLightList);
#glsl iIndexToSpotLightList = atomicAdd(iOpaqueSpotLightCountIntersectingTileFrustum, 1);
    
    // Make sure we won't access out of bound.
    if (iIndexToSpotLightList < TILE_SPOT_LIGHT_INDEX_LIST_SIZE) {
        tileOpaqueSpotLightIndexList[iIndexToSpotLightList] = iSpotLightIndex;
    }
}

// --------------------------------------------------------------------------------------------------------------------
//                                              light culling functions
// --------------------------------------------------------------------------------------------------------------------

/** Does forward tiled light culling for the specified thread in a group. */
void cullLightsForTile(
    uint iThreadIdXInDispatch,
    uint iThreadIdYInDispatch,
    uint iThreadGroupXIdInDispatch,
    uint iThreadGroupYIdInDispatch,
    uint iThreadIdInGroup) {
    // Sources:
    // - Presentation "DirectX 11 Rendering in Battlefield 3" (2011) by Johan Andersson, DICE.
    // - "Forward+: A Step Toward Film-Style Shading in Real Time", Takahiro Harada (2012).
    
    // Prepare variable to store depth.
    float pixelDepth = 1.0F;
    
    // Avoid reading depth and running calculations out of bounds because if width/height is not evenly divisible
    // by the thread group size, in C++ we will `ceil` the result and dispatch slightly more threads.
    // So some thread groups can have some threads that will be out of bounds.
    if (iThreadIdXInDispatch >= screenToViewData.iRenderTargetWidth || iThreadIdYInDispatch >= screenToViewData.iRenderTargetHeight) {
        return;
    }
    
    // Get depth of this pixel.
    pixelDepth =
#hlsl depthTexture.Load(int3(iThreadIdXInDispatch, iThreadIdYInDispatch, 0)).r;
#glsl texelFetch(depthTexture, ivec2(iThreadIdXInDispatch, iThreadIdYInDispatch), 0).r;
    
    if (iThreadIdInGroup == 0) {
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
    
    // Each iteration will now process `LIGHT_GRID_TILE_SIZE_IN_PIXELS^2` lights in parallel for the tile:
    // thread 0 processes lights: 0, LIGHT_GRID_TILE_SIZE_IN_PIXELS^2, (LIGHT_GRID_TILE_SIZE_IN_PIXELS^2) * 2, etc.
    // thread 1 processes lights: 1, LIGHT_GRID_TILE_SIZE_IN_PIXELS^2 + 1, etc.
    
    // Cull point lights.
    for (uint i = iThreadIdInGroup; i < generalLightingData.iPointLightCountsInCameraFrustum; i += LIGHT_GRID_TILE_SIZE_IN_PIXELS * LIGHT_GRID_TILE_SIZE_IN_PIXELS) {
        // Get point light index.
        const uint iPointLightIndex =
#hlsl         pointLightsInCameraFrustumIndices[i];
#glsl         pointLightsInCameraFrustumIndices.array[i];
        
        // Get point light.
        PointLight pointLight =
#hlsl         pointLights[iPointLightIndex];
#glsl         pointLights.array[iPointLightIndex];
        
        // Calculate light view space position.
        vec3 lightViewSpacePosition = 
#hlsl mul(frameData.viewMatrix, float4(pointLight.position.xyz, 1.0F)).xyz;
#glsl (frameData.viewMatrix * vec4(pointLight.position.xyz, 1.0F)).xyz;
        
        // Construct a sphere according to point light's radius for sphere/frustum test.
        Sphere pointLightSphereViewSpace;
        pointLightSphereViewSpace.center = lightViewSpacePosition;
        pointLightSphereViewSpace.radius = pointLight.distance;
        
        // First test for transparent geometry since frustum for transparent objects has more Z space.
        if (!isSphereInsideFrustum(pointLightSphereViewSpace, tileFrustum, nearClipPlaneViewSpace, tileMaxDepthViewSpace)) {
            continue;
        }
        
        // Append this light to transparent geometry.
        
        // Now we know that the light is inside frustum for transparent objects, frustum for opaque objects is
        // the same expect it has smaller Z range so it's enough to just test sphere/plane.
        if (isSphereBehindPlane(pointLightSphereViewSpace, minDepthPlaneViewSpace)) {
            continue;
        }
        
        // Append this light to opaque geometry.
        addPointLightToTileOpaqueLightIndexList(iPointLightIndex);
    }
    
    // Cull spot lights.
    for (uint i = iThreadIdInGroup; i < generalLightingData.iSpotlightCountsInCameraFrustum; i += LIGHT_GRID_TILE_SIZE_IN_PIXELS * LIGHT_GRID_TILE_SIZE_IN_PIXELS) {
        // Get spotlight index.
        const uint iSpotlightIndex =
#hlsl         spotlightsInCameraFrustumIndices[i];
#glsl         spotlightsInCameraFrustumIndices.array[i];
        
        // Get spotlight.
        Spotlight spotlight =
#hlsl         spotlights[iSpotlightIndex];
#glsl         spotlights.array[iSpotlightIndex];
        
        // Calculate light view space position.
        vec3 lightViewSpacePosition = 
#hlsl mul(frameData.viewMatrix, float4(spotlight.position.xyz, 1.0F)).xyz;
#glsl (frameData.viewMatrix * vec4(spotlight.position.xyz, 1.0F)).xyz;
        
        // Calculate light view space direction.
        vec3 lightViewSpaceDirection = 
#hlsl mul(frameData.viewMatrix, float4(spotlight.direction.xyz, 0.0F)).xyz;
#glsl (frameData.viewMatrix * vec4(spotlight.direction.xyz, 0.0F)).xyz;
        
        // Construct a cone according to spotlight data for cone/frustum test.
        Cone spotlightConeViewSpace;
        spotlightConeViewSpace.location = lightViewSpacePosition;
        spotlightConeViewSpace.height = spotlight.distance;
        spotlightConeViewSpace.direction = lightViewSpaceDirection;
        spotlightConeViewSpace.bottomRadius = spotlight.coneBottomRadius;
        
        // First test for transparent geometry since frustum for transparent objects has more Z space.
        if (!isConeInsideFrustum(spotlightConeViewSpace, tileFrustum, nearClipPlaneViewSpace, tileMaxDepthViewSpace)) {
            continue;
        }
        
        // Now we know that the light is inside frustum for transparent objects, frustum for opaque objects is
        // the same expect it has smaller Z range so it's enough to just test cone/plane.
        if (isConeBehindPlane(spotlightConeViewSpace, minDepthPlaneViewSpace)) {
            continue;
        }
        
        // Append this light to opaque geometry.
        addSpotLightToTileOpaqueLightIndexList(iSpotlightIndex);
    }
    
    // Wait for all group threads to cull lights and finish writing to groupshared memory.
#hlsl GroupMemoryBarrierWithGroupSync();
#glsl groupMemoryBarrier(); barrier();
    
    if (iThreadIdInGroup == 0) {
        // Write non-culled lights to light grid and reserve space in global light list.
        reserveSpaceForNonCulledTileLightsInLightGridAndList(iThreadGroupXIdInDispatch, iThreadGroupYIdInDispatch);
    }
    
    // Wait for all group threads before updating global light index list.
#hlsl GroupMemoryBarrierWithGroupSync();
#glsl groupMemoryBarrier(); barrier();
    
    // Write local light index list into the global light index list.
    // Start with opaque geometry.
    
    // Write point lights.
    for (uint i = iThreadIdInGroup; i < iOpaquePointLightCountIntersectingTileFrustum; i += LIGHT_GRID_TILE_SIZE_IN_PIXELS * LIGHT_GRID_TILE_SIZE_IN_PIXELS) {
#hlsl    opaquePointLightIndexList
#glsl    opaquePointLightIndexList.array
        [iOpaquePointLightIndexListStartOffset + i] = tileOpaquePointLightIndexList[i];
    }
    
    // Write spot lights.
    for (uint i = iThreadIdInGroup; i < iOpaqueSpotLightCountIntersectingTileFrustum; i += LIGHT_GRID_TILE_SIZE_IN_PIXELS * LIGHT_GRID_TILE_SIZE_IN_PIXELS) {
#hlsl    opaqueSpotLightIndexList
#glsl    opaqueSpotLightIndexList.array
        [iOpaqueSpotLightIndexListStartOffset + i] = tileOpaqueSpotLightIndexList[i];
    }
}