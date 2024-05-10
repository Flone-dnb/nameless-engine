#include "../../../include/light_culling/CountersData.glsl"

[numthreads(1, 1, 1)]
/** Reset counters for light culling. */
void main() {
    resetGlobalLightIndexListCounters();
}