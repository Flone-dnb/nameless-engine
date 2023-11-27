// This file is used by compute shaders that need to calculate frustum per light grid tile for Forward+ light culling.

#include "../Shapes.glsl"
#include "../CoordinateSystemConversion.glsl"

/** Stores some additional information (some information not available as built-in semantics). */
#glsl layout(binding = 0) uniform ComputeInfo {
#hlsl struct ComputeInfo{
    /** Total number of thread groups dispatched in the X direction. */
    uint iThreadGroupCountX;

    /** Total number of thread groups dispatched in the Y direction. */
    uint iThreadGroupCountY;

    /** Total number of tiles in the X direction. */
    uint iTileCountX;

    /** Total number of tiles in the Y direction. */
    uint iTileCountY;

    /** Maximum depth value. */
    float maxDepth;
#glsl } computeInfo;
#hlsl }; ConstantBuffer<ComputeInfo> computeInfo : register(b0, space5);

/** Data that we need to convert coordinates from screen space to view space. */
#glsl layout(binding = 1) uniform ScreenToViewData {
#hlsl struct ScreenToViewData{
    /** Inverse of the projection matrix. */
    mat4 inverseProjectionMatrix;

    /** Width of the underlying render image (might be smaller that the actual screen size). */
    uint iRenderTargetWidth;

    /** Height of the underlying render image (might be smaller that the actual screen size). */
    uint iRenderTargetHeight;
#glsl } screenToViewData;
#hlsl }; ConstantBuffer<ScreenToViewData> screenToViewData : register(b1, space5);

/** Calculated grid of frustums in view space. */
#hlsl RWStructuredBuffer<Frustum> calculatedFrustums : register(u0, space5);
#glsl{
layout(std140, binding = 2) buffer CalculatedFrustumsBuffer{
    Frustum array[];
} calculatedFrustums;
}

/**
 * Calculates a frustum in view space from a grid tile in screen space.
 *
 * @param iTileXPosition          X position of the grid tile.
 * @param iTileYPosition          Y position of the grid tile.
 * @param iTileSizeInPixels       Size of one grid tile in pixels.
 * @param tileZ                   Z coordinate for tile in NDC space (since there's really no Z in screen space).
 * @param iRenderTargetWidth      Width of the viewport (might be smaller that the actual screen size).
 * @param iRenderTargetHeight     Height of the viewport (might be smaller that the actual screen size).
 * @param inverseProjectionMatrix Inverse of the projection matrix.
 *
 * @return Frustum in view space for the specified grid tile.
 */
Frustum calculateFrustumInViewSpaceForGridTileInScreenSpace(
    uint iTileXPosition,
    uint iTileYPosition,
    uint iTileSizeInPixels,
    float tileZ,
    uint iRenderTargetWidth,
    uint iRenderTargetHeight,
    mat4 inverseProjectionMatrix){
    // Calculate 4 corner points of frustum's far clip plane.
    vec3 farClipPlaneCornersScreenSpace[4];

    // Points that we will now calculate will be in screen space because we know the size of one grid tile in pixels,
    // these coordinates will be in range [0..SCREEN_WIDTH; 0..SCREEN_HEIGHT; Z].

    // Calculate top-left point.
    farClipPlaneCornersScreenSpace[0] = vec3(vec2(iTileXPosition, iTileYPosition) * iTileSizeInPixels, tileZ);

    // Calculate top-right point.
    farClipPlaneCornersScreenSpace[1] = vec3(vec2(iTileXPosition + 1, iTileYPosition) * iTileSizeInPixels, tileZ);

    // Calculate bottom-left point.
    farClipPlaneCornersScreenSpace[2] = vec3(vec2(iTileXPosition, iTileYPosition + 1) * iTileSizeInPixels, tileZ);

    // Calculate bottom-right point.
    farClipPlaneCornersScreenSpace[3] = vec3(vec2(iTileXPosition + 1, iTileYPosition + 1) * iTileSizeInPixels, tileZ);

    // Now we need to transform these points from screen space to view space
    // because later we would want our frustums to be in view space.
    vec3 farClipPlaneCornersViewSpace[4];
    for (int i = 0; i < 4; i++){
        // Convert screen space to view space.
        farClipPlaneCornersViewSpace[i] = convertScreenSpaceToViewSpace(
            farClipPlaneCornersScreenSpace[i],
            iRenderTargetWidth,
            iRenderTargetHeight,
            inverseProjectionMatrix).xyz;
    }

    // Prepare camera location constant.
    const vec3 cameraLocationViewSpace = vec3(0.0F, 0.0F, 0.0F);

    // Now build frustum.
    Frustum frustum;
    frustum.planes[0] = calculatePlaneFromTriangle(cameraLocationViewSpace, farClipPlaneCornersViewSpace[0], farClipPlaneCornersViewSpace[2]);
    frustum.planes[1] = calculatePlaneFromTriangle(cameraLocationViewSpace, farClipPlaneCornersViewSpace[3], farClipPlaneCornersViewSpace[1]);
    frustum.planes[2] = calculatePlaneFromTriangle(cameraLocationViewSpace, farClipPlaneCornersViewSpace[1], farClipPlaneCornersViewSpace[0]);
    frustum.planes[3] = calculatePlaneFromTriangle(cameraLocationViewSpace, farClipPlaneCornersViewSpace[2], farClipPlaneCornersViewSpace[3]);

    return frustum;
}