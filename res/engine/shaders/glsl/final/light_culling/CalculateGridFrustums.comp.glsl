#version 450

#include "../../../include/light_culling/CalculateGridTileFrustum.glsl"

/**
 * Defines how much threads should be executed in the X and the Y dimensions.
 * This macro also defines how much pixels there are in one grid tile.
 */
#ifndef LIGHT_GRID_TILE_SIZE_IN_PIXELS
FAIL;
#endif

/** 1 thread calculates 1 frustum (1 thread per grid tile, not per pixel - see dispatch call). */
layout (local_size_x = LIGHT_GRID_TILE_SIZE_IN_PIXELS, local_size_y = LIGHT_GRID_TILE_SIZE_IN_PIXELS, local_size_z = 1) in;
void main(){
    // Make sure we don't calculate frustums out of bounds
    // (this may happen if the screen size is not evenly divisible by the thread group size).
    if (gl_GlobalInvocationID.x >= computeInfo.iTileCountX || gl_GlobalInvocationID.y >= computeInfo.iTileCountY){
        return;
    }

    // Calculate index into the resulting buffer to write the calculated frustum.
    const uint iFrustumIndex = (gl_GlobalInvocationID.y * computeInfo.iTileCountX) + gl_GlobalInvocationID.x;

    // Calculate tile frustum and write it to the resulting buffer.
    calculatedFrustums.array[iFrustumIndex] = calculateFrustumInViewSpaceForGridTileInScreenSpace(
        gl_GlobalInvocationID.x, // tile X
        gl_GlobalInvocationID.y, // tile Y
        LIGHT_GRID_TILE_SIZE_IN_PIXELS,
        screenToViewData.iRenderTargetWidth,
        screenToViewData.iRenderTargetHeight,
        screenToViewData.inverseProjectionMatrix);
}