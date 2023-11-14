#include "../../include/Shapes.hlsl"

/**
 * Defines how much threads should be executed in the X and the Y dimensions of a grid tile.
 * This macro also defines how much pixels there are in one grid tile.
 */
#ifndef THREADS_IN_GROUP_XY
_Static_assert(false, "required macro not defined");
#endif

/** Built-in semantics that we use. */
struct ComputeShaderInput
{
    /** 3D index of a thread group in the dispatch. */
    uint3 groupID : SV_GroupID;

    /** 3D index of a thread in a group. */
    uint3 groupThreadID : SV_GroupThreadID;

    /** 3D index of a thread in the dispatch. */
    uint3 dispatchThreadID : SV_DispatchThreadID;
    
    /** Flattened index of a thread within a group. */
    uint iGroupIndex : SV_GroupIndex;
};

/** Stores some additional information (some information not available as built-in semantics). */
struct ComputeInfo{
    /** Total number of thread groups dispatched. 4th component is not used. */
    uint4 threadGroupCount;

    /**
     * Total number of threads dispatched that will be used. 4th component is not used.
     *
     * @remark This value may be less than the actual number of threads dispatched if the screen size is not evenly divisible
     * by the thread group size.
     */
    uint4 threadsInUseCount;

    /** Maximum depth value. */
    float maxDepth;
};
ConstantBuffer<DispatchInfo> computeInfo : register(b0, space5);

/** Data that we need to convert coordinates from screen space to view space. */
struct ScreenToViewData{
    /** Inverse of the projection matrix. */
    float4x4 inverseProjectionMatrix;

    /** Width and height of the viewport (might be smaller that the actual screen size). */
    float2 renderResolution;
};
ConstantBuffer<ScreenToViewData> screenToViewData : register(b1, space5);

/** Calculated grid of frustums in view space. */
RWStructuredBuffer<Frustum> calculatedFrustums : register(u0, space5);

/**
 * Converts the specified coordinates from NDC space to view space.
 *
 * @param ndcSpaceCoordinates     Coordinates in screen space with depth in NDC space (since there's really no depth in screen space).
 * @param inverseProjectionMatrix Inverse of the projection matrix.
 *
 * @return Coordinates in view space.
 */
float4 convertNdcSpaceToViewSpace(float3 ndcSpaceCoordinates, float4x4 inverseProjectionMatrix){
    // Undo projection.
    float4 viewSpaceCoordinates
        = mul(inverseProjectionMatrix, float4(ndcSpaceCoordinates, 1.0F)); // NDC `w` is always 1 because after perspective divide
                                                                           // (perspective divide does xyzw/w)

    // Undo perspective divide.
    viewSpaceCoordinates = viewSpaceCoordinates / viewSpaceCoordinates.w;

    return viewSpaceCoordinates;
}

/**
 * Converts the specified coordinates from screen space to view space.
 *
 * @param screenSpaceCoordinates  Coordinates in screen space with depth in NDC space (since there's really no depth in screen space).
 * @param renderResolution        Width and height of the viewport (might be smaller that the actual screen size).
 * @param inverseProjectionMatrix Inverse of the projection matrix.
 *
 * @return Coordinates in view space.
 */
float4 convertScreenSpaceToViewSpace(float3 screenSpaceCoordinates, float2 renderResolution, float4x4 inverseProjectionMatrix){
    // Convert screen coordinates to normalized coordinates in range [0..1; 0..1].
    float2 screenNormalized = screenSpaceCoordinates.xy / renderResolution;

    // Calculate coordinates in NDC space.
    float3 ndcSpaceCoordinates = float3(
        screenNormalized.x * 2.0F - 1.0F,  // converts from [0..1] to [-1..1]
        -2.0F * screenNormalized.y + 1.0F, // "flips" (not an actual flip) Y since in screen space it goes down but
                                           // in NDC it goes up and converts from [0..1] to [-1..1]
        screenSpaceCoordinates.z);

    return convertNdcSpaceToViewSpace(ndcSpaceCoordinates, inverseProjectionMatrix);
}

/** 1 thread calculates 1 frustum (1 thread per grid tile, not per pixel). */
[numthreads(THREADS_IN_GROUP_XY, THREADS_IN_GROUP_XY, 1 )]
void csGridFrustums(ComputeShaderInput input){
    // Make sure we don't calculate frustums out of bounds
    // (this may happen if the screen size is not evenly divisible by the thread group size).
    if (input.dispatchThreadID.x >= computeInfo.threadsInUseCount.x || input.dispatchThreadID.y >= computeInfo.threadsInUseCount.y){
        return;
    }

    // Calculate 4 corner points of frustum's far clip plane.
    float3 farClipPlaneCornersScreenSpace[4];

    // Since in this shader we execute a thread per grid tile (not pixel), points that we will now calculate will be
    // calculated in screen space because we know the size of one grid tile in pixels, these coordinates will be in
    // range [0..SCREEN_WIDTH; 0..SCREEN_HEIGHT; 1.0F], where the Z-coordinate points in the positive direction because
    // our camera is looking down the positive Z axis.

    // Prepare some constants so that it will be easier for us to understand the code below.
    const float screenZ = computeInfo.maxDepth; // far clip plane Z in screen space
    const float tileSizeInPixels = THREADS_IN_GROUP_XY;
    const float2 tileIndex = input.dispatchThreadID.xy; // index of the tile in the grid

    // Calculate top-left point.
    farClipPlaneCornersScreenSpace[0] = float3(tileIndex * tileSizeInPixels, screenZ);

    // Calculate top-right point.
    farClipPlaneCornersScreenSpace[1] = float3(float2(tileIndex.x + 1, tileIndex.y) * tileSizeInPixels, screenZ);

    // Calculate bottom-left point.
    farClipPlaneCornersScreenSpace[2] = float3(float2(tileIndex.x, tileIndex.y + 1) * tileSizeInPixels, screenZ);

    // Calculate bottom-right point.
    farClipPlaneCornersScreenSpace[3] = float3(float2(tileIndex.x + 1, tileIndex.y + 1) * tileSizeInPixels, screenZ);

    // Now we need to transform these points from screen space to view space
    // because later we would want our frustums to be in view space.
    float3 farClipPlaneCornersViewSpace[4];
    for (int i = 0; i < 4; i++){
        // Convert screen space to view space.
        farClipPlaneCornersViewSpace[i] = convertScreenSpaceToViewSpace(
            farClipPlaneCornersScreenSpace[i],
            screenToViewData.renderResolution,
            screenToViewData.inverseProjectionMatrix).xyz;
    }

    // Prepare camera location constant.
    const float3 cameraLocationViewSpace = float3(0.0F, 0.0F, 0.0F);

    // Now build frustum.
    Frustum frustum;
    frustum.planes[0] = calculatePlaneFromTriangle(cameraLocationViewSpace, farClipPlaneCornersViewSpace[2], farClipPlaneCornersViewSpace[0]);
    frustum.planes[1] = calculatePlaneFromTriangle(cameraLocationViewSpace, farClipPlaneCornersViewSpace[1], farClipPlaneCornersViewSpace[3]);
    frustum.planes[2] = calculatePlaneFromTriangle(cameraLocationViewSpace, farClipPlaneCornersViewSpace[0], farClipPlaneCornersViewSpace[1]);
    frustum.planes[3] = calculatePlaneFromTriangle(cameraLocationViewSpace, farClipPlaneCornersViewSpace[3], farClipPlaneCornersViewSpace[2]);

    // Calculate index to write our frustum to.
    uint iFrustumIndex = (input.dispatchThreadID.y * computeInfo.threadsInUseCount.x) + input.dispatchThreadID.x;

    // Write our frustum to the resulting buffer.
    calculatedFrustums[iFrustumIndex] = frustum;
}