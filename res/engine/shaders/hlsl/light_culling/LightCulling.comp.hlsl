#include "../../include/light_culling/LightCulling.glsl"

#ifndef THREADS_IN_GROUP_XY
_Static_assert(false, "thread count in group - macro not defined");
#endif

[numthreads(THREADS_IN_GROUP_XY, THREADS_IN_GROUP_XY, 1 )]
void csLightCulling(uint3 dispatchThreadID : SV_DispatchThreadID){
    // Sources:
    // - Presentation "DirectX 11 Rendering in Battlefield 3" (2011) by Johan Andersson, DICE.
    // - "Forward+: A Step Toward Film-Style Shading in Real Time", Takahiro Harada (2012).
    
    // Get depth.
    float depth = depthTexture.Load(int3(dispatchThreadID.xy, 0)).r;
}