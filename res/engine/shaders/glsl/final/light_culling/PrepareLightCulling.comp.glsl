#version 450

#include "../../../include/light_culling/CountersData.glsl"

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
/** Resets counters for light culling. */
void main() {
    resetGlobalLightIndexListCounters();
}