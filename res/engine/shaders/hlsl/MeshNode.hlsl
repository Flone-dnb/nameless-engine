#include "Base.hlsl"

#ifdef TEXTURE_FILTERING_POINT
    SamplerState samplerPointWrap       : register(s0, space5);
#elif TEXTURE_FILTERING_LINEAR
    SamplerState samplerLinearWrap      : register(s0, space5);
#elif TEXTURE_FILTERING_ANISOTROPIC
    SamplerState samplerAnisotropicWrap : register(s0, space5);
#endif

#ifdef RECEIVE_DYNAMIC_SHADOWS
    SamplerComparisonState samplerShadowMap : register(s1, space5);
#endif

// will be used later
// #ifdef USE_DIFFUSE_TEXTURE
//     Texture2D diffuseTexture : register(t0, space5);
// #endif

/** Describes MeshNode's constants. */
cbuffer meshData : register(b1, space5)
{
    /** Matrix that transforms vertices from mesh local space to world space. */
    float4x4 worldMatrix; 
    
    // don't forget to pad to 4 floats
};

VertexOut vsMeshNode(VertexIn vertexIn)
{
    VertexOut vertexOut;

    // Calculate world position and normal.
    vertexOut.worldPosition = mul(float4(vertexIn.localPosition, 1.0f), worldMatrix);
    vertexOut.worldNormal = mul(vertexIn.localNormal, (float3x3)worldMatrix);

    // Transform position to homogeneous clip space.
    vertexOut.viewPosition = mul(vertexOut.worldPosition, viewProjectionMatrix);

    // Copy UV.
    vertexOut.uv = vertexIn.uv;

    return vertexOut;
}

float4 psMeshNode(VertexOut pin) : SV_Target
{
    float4 diffuseColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

// will be used later
// #ifdef USE_DIFFUSE_TEXTURE
//     // Apply texture filtering.
//     #ifdef TEXTURE_FILTERING_POINT
//         diffuseColor = diffuseTexture.Sample(samplerPointWrap, pin.uv);
//     #elif TEXTURE_FILTERING_LINEAR
//         diffuseColor = diffuseTexture.Sample(samplerLinearWrap, pin.uv);
//     #elif TEXTURE_FILTERING_ANISOTROPIC
//         diffuseColor = diffuseTexture.Sample(samplerAnisotropicWrap, pin.uv);
//     #endif
// #endif

    return diffuseColor;
}