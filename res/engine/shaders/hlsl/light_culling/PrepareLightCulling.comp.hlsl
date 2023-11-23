#include "../../include/light_culling/CountersData.glsl"

[numthreads(1, 1, 1)]
void main(){
    // Reset counters for light culling.
    resetGlobalLightIndexListCounters();
}