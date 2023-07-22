#include "Base.hlsl"

#ifdef USE_DIFFUSE_TEXTURE
   SamplerState textureSampler : register(s0, space5);
#endif

#ifdef USE_DIFFUSE_TEXTURE
    Texture2D diffuseTexture : register(t0, space5);
#endif

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
};

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

#ifdef USE_DIFFUSE_TEXTURE
    outputColor *= diffuseTexture.Sample(textureSampler, pin.uv);
#endif

#ifdef USE_MATERIAL_TRANSPARENCY
    outputColor.w *= opacity;
    // Early exit (if possible).
    clip(outputColor.w - 0.01F);
#endif

    return outputColor;
}