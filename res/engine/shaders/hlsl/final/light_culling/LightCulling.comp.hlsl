#include "../../../include/light_culling/LightCulling.glsl"

/**
 * Defines how much threads should be executed in the X and the Y dimensions.
 * This macro also defines how much pixels there are in one grid tile.
 */
#ifndef LIGHT_GRID_TILE_SIZE_IN_PIXELS
_Static_assert(false, "thread count in group - macro not defined");
#endif

/** 1 thread per pixel in a tile. 1 thread group per tile. */
[numthreads(LIGHT_GRID_TILE_SIZE_IN_PIXELS, LIGHT_GRID_TILE_SIZE_IN_PIXELS, 1)]
void main(uint3 threadIdInDispatch : SV_DispatchThreadID, uint iThreadIdInGroup : SV_GroupIndex, uint3 groupIdInDispatch : SV_GroupID) {
    // Cull lights.
    cullLightsForTile(
        threadIdInDispatch.x,
        threadIdInDispatch.y,
        groupIdInDispatch.x,
        groupIdInDispatch.y,
        iThreadIdInGroup);
}