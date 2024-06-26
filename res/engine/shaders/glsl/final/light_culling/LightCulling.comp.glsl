#version 450

#include "../../../include/light_culling/LightCulling.glsl"

/**
 * Defines how much threads should be executed in the X and the Y dimensions.
 * This macro also defines how much pixels there are in one grid tile.
 */
#ifndef LIGHT_GRID_TILE_SIZE_IN_PIXELS
    FAIL;
#endif

layout (local_size_x = LIGHT_GRID_TILE_SIZE_IN_PIXELS, local_size_y = LIGHT_GRID_TILE_SIZE_IN_PIXELS, local_size_z = 1) in;
/** 1 thread per pixel in a tile. 1 thread group per tile. */
void main() {
    // Cull lights.
    cullLightsForTile(
        gl_GlobalInvocationID.x,
        gl_GlobalInvocationID.y,
        gl_WorkGroupID.x,
        gl_WorkGroupID.y,
        gl_LocalInvocationIndex);
}