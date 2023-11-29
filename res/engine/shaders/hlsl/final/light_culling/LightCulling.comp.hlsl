#include "../../../include/light_culling/LightCulling.glsl"

/**
 * Defines how much threads should be executed in the X and the Y dimensions.
 * This macro also defines how much pixels there are in one grid tile.
 */
#ifndef LIGHT_GRID_TILE_SIZE_IN_PIXELS
_Static_assert(false, "thread count in group - macro not defined");
#endif

/** 1 thread per pixel in a tile. 1 thread group per tile. */
[numthreads(LIGHT_GRID_TILE_SIZE_IN_PIXELS, LIGHT_GRID_TILE_SIZE_IN_PIXELS, 1 )]
void main(uint3 threadIdInDispatch : SV_DispatchThreadID, uint iThreadIdInGroup : SV_GroupIndex, uint3 groupIdInDispatch : SV_GroupID){
    // Prepare variable to store depth.
    float depth = 1.0F;

    // Avoid reading depth out of bounds because if width/height is not evenly divisible by the thread group size
    // we will `ceil` the result and dispatch slightly more threads. So some thread groups can have some threads
    // that will be out of bounds.
    if (threadIdInDispatch.x < screenToViewData.iRenderTargetWidth && threadIdInDispatch.y < screenToViewData.iRenderTargetHeight){
        // Get depth of this pixel.
        depth = depthTexture.Load(int3(threadIdInDispatch.xy, 0)).r;
    }

    // Cull lights.
    cullLightsForTile(depth, groupIdInDispatch.x, groupIdInDispatch.y, iThreadIdInGroup);
}