#glsl{
#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : enable
}

#include "data/FrameData.glsl"

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
    float4 position       : SV_POSITION; // in vertex shader it's in clip space and in pixel shader it's in screen space
    float4 worldPosition  : POSITION;    // position in world space
    float3 worldNormal    : NORMAL;      // normal in world space
    float2 uv             : UV;
};
}

/** Initial (empty) push/root constants definition in order for later `#additional_..._constants` to work. */
#glsl{
layout(push_constant) uniform PushConstants{
} constants;
}
#hlsl{
struct RootConstants{
}; ConstantBuffer<RootConstants> constants : register(b0, space8);
}