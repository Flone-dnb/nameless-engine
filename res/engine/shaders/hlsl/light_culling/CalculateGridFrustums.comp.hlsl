#include "../../include/light_culling/CalculateGridTileFrustum.glsl"

/**
 * Defines how much threads should be executed in the X and the Y dimensions.
 * This macro also defines how much pixels there are in one grid tile.
 */
#ifndef LIGHT_GRID_TILE_SIZE_IN_PIXELS
_Static_assert(false, "thread count in group - macro not defined");
#endif

/** 1 thread calculates 1 frustum (1 thread per grid tile, not per pixel - see dispatch call). */
[numthreads(LIGHT_GRID_TILE_SIZE_IN_PIXELS, LIGHT_GRID_TILE_SIZE_IN_PIXELS, 1 )]
void csGridFrustum(uint3 dispatchThreadID : SV_DispatchThreadID){
    // Make sure we don't calculate frustums out of bounds
    // (this may happen if the screen size is not evenly divisible by the thread group size).
    if (dispatchThreadID.x >= computeInfo.iTileCountX || dispatchThreadID.y >= computeInfo.iTileCountY){
        return;
    }

    // Calculate index into the resulting buffer to write the calculated frustum.
    const uint iFrustumIndex = (dispatchThreadID.y * computeInfo.iTileCountX) + dispatchThreadID.x;

    // Calculate tile frustum and write it to the resulting buffer.
    calculatedFrustums[iFrustumIndex] = calculateFrustumInViewSpaceForGridTileInScreenSpace(
        dispatchThreadID.x, // tile X
        dispatchThreadID.y, // tile Y
        LIGHT_GRID_TILE_SIZE_IN_PIXELS,
        computeInfo.maxDepth,     // far clip plane Z in screen space
        screenToViewData.iRenderTargetWidth,
        screenToViewData.iRenderTargetHeight,
        screenToViewData.inverseProjectionMatrix);
}