// This file is expected to be included by all shaders.

#glsl{
#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
}

/** Stores frame-global constants. */
#glsl layout(binding = 0) uniform FrameData {
#hlsl struct FrameData{
    /** Camera's view matrix multiplied by camera's projection matrix. */
    mat4 viewProjectionMatrix;

    /** Camera's world location. 4th component is not used. */
    vec4 cameraPosition;

    /** Time that has passed since the last frame in seconds (i.e. delta time). */
    float timeSincePrevFrameInSec;

    /** Time since the first window was created (in seconds). */
    float totalTimeInSec;
#glsl } frameData;
#hlsl }; ConstantBuffer<FrameData> frameData : register(b0, space5);

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
    float4 viewPosition            : SV_POSITION; // position in view space
    float4 worldPosition           : POSITION;    // position in world space
    float3 worldNormal             : NORMAL;      // normal in world space
    float2 uv                      : UV;
};
}