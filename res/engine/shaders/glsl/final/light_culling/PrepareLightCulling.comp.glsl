#version 450

#include "../../../include/light_culling/CountersData.glsl"

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main(){
    // Reset counters for light culling.
    resetGlobalLightIndexListCounters();
}