#include "../../include/light_culling/LightCulling.glsl"

/**
 * Defines how much threads should be executed in the X and the Y dimensions.
 * This macro also defines how much pixels there are in one grid tile.
 */
#ifndef THREADS_IN_GROUP_XY
_Static_assert(false, "thread count in group - macro not defined");
#endif

/** 1 thread per pixel in a tile. 1 thread group per tile. */
[numthreads(THREADS_IN_GROUP_XY, THREADS_IN_GROUP_XY, 1 )]
void csLightCulling(uint3 threadIdInDispatch : SV_DispatchThreadID, uint threadIdInGroup : SV_GroupIndex, uint3 groupIdInDispatch : SV_GroupID){
    // Sources:
    // - Presentation "DirectX 11 Rendering in Battlefield 3" (2011) by Johan Andersson, DICE.
    // - "Forward+: A Step Toward Film-Style Shading in Real Time", Takahiro Harada (2012).
    
    // Get depth of this pixel.
    float depth = depthTexture.Load(int3(threadIdInDispatch.xy, 0)).r;

    if (threadIdInGroup == 0){
        // Only one thread in the group should initialize group shared variables.
        initializeGroupSharedVariables(groupIdInDispatch.x, groupIdInDispatch.y);
    }

    // Make sure all group shared writes were finished and all threads from the group reached this line.
    GroupMemoryBarrierWithGroupSync();
}