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
    
    // don't forget to pad to 4 floats (if needed)
};

/** Describes Material's constants. */
cbuffer materialData : register(b2, space5)
{
    /** Fill color. */
    float3 diffuseColor;

    /** Opacity (when material transparency is used). */
    float opacity;

    // don't forget to pad to 4 floats (if needed)
}

VertexOut vsMeshNode(VertexIn vertexIn)
{
    VertexOut vertexOut;

    // Calculate world position and normal.
    vertexOut.worldPosition = mul(worldMatrix, float4(vertexIn.localPosition, 1.0F));
    vertexOut.worldNormal = normalize(mul((float3x3)worldMatrix, vertexIn.localNormal));

    // Transform position to homogeneous clip space.
    vertexOut.viewPosition = mul(viewProjectionMatrix, vertexOut.worldPosition);

    // Copy UV.
    vertexOut.uv = vertexIn.uv;

    return vertexOut;
}

float4 psMeshNode(VertexOut pin) : SV_Target
{
    float4 outputColor = float4(diffuseColor, 1.0F);

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

#ifdef USE_MATERIAL_TRANSPARENCY
    // Early exit (if possible).
    clip(opacity - 0.01F);
    outputColor.a = opacity;
#endif

    return outputColor;
}