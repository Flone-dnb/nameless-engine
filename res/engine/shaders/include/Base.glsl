// This file is expected to be included by most shaders.

#glsl{
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
}

#include "FrameData.glsl"

#hlsl{
/** Describes vertex shader input data. */
struct VertexIn
{
    float3 localPosition : POSITION;    // position in local space
    float3 localNormal   : NORMAL;      // normal in local space
    float2 uv            : UV;
};

/** Describes pixel shader input data. */
struct VertexOut
{
    float4 clipSpacePosition       : SV_POSITION; // position in homogeneous clip space
    float4 worldPosition           : POSITION;    // position in world space
    float3 worldNormal             : NORMAL;      // normal in world space
    float2 uv                      : UV;
};
}