#version 450

#include "../../../include/light_culling/LightCulling.glsl"

/**
 * Defines how much threads should be executed in the X and the Y dimensions.
 * This macro also defines how much pixels there are in one grid tile.
 */
#ifndef LIGHT_GRID_TILE_SIZE_IN_PIXELS
FAIL;
#endif

/** 1 thread per pixel in a tile. 1 thread group per tile. */
layout (local_size_x = LIGHT_GRID_TILE_SIZE_IN_PIXELS, local_size_y = LIGHT_GRID_TILE_SIZE_IN_PIXELS, local_size_z = 1) in;
void main(){
    // Prepare variable to store depth.
    float depth = 1.0F;

    // Avoid reading depth out of bounds because if width/height is not evenly divisible by the thread group size
    // we will `ceil` the result and dispatch slightly more threads. So some thread groups can have some threads
    // that will be out of bounds.
    if (gl_GlobalInvocationID.x < screenToViewData.iRenderTargetWidth && gl_GlobalInvocationID.y < screenToViewData.iRenderTargetHeight){
        // Get depth of this pixel.
        depth = texelFetch(depthTexture, ivec2(gl_GlobalInvocationID.x, gl_GlobalInvocationID.y), 0).r;
    }

    // Cull lights.
    cullLightsForTile(depth, gl_WorkGroupID.x, gl_WorkGroupID.y, gl_LocalInvocationIndex);
}