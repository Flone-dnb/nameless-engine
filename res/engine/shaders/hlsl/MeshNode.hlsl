#include "Base.hlsl"

#ifdef PS_USE_DIFFUSE_TEXTURE
   SamplerState textureSampler : register(s0, space5);
#endif

#ifdef PS_USE_DIFFUSE_TEXTURE
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
    // Set diffuse color.
    float4 outputColor = float4(diffuseColor, 1.0F);

#ifdef PS_USE_DIFFUSE_TEXTURE
    // Apply diffuse texture.
    outputColor *= diffuseTexture.Sample(textureSampler, pin.uv);
#endif

    // Apply ambient light.
    outputColor.rgb *= ambientLight;

#ifdef PS_USE_MATERIAL_TRANSPARENCY
    // Apply opacity.
    outputColor.a *= opacity;
#endif

    return outputColor;
}