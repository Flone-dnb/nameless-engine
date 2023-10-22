#include "Base.hlsl"
#include "Lighting.hlsl"

#ifdef PS_USE_DIFFUSE_TEXTURE
   SamplerState textureSampler : register(s0, space5);
#endif

#ifdef PS_USE_DIFFUSE_TEXTURE
    Texture2D diffuseTexture : register(t1, space5);
#endif

/** Describes MeshNode's constants. */
cbuffer meshData : register(b2, space5)
{
    /** Matrix that transforms vertices from mesh local space to world space. */
    float4x4 worldMatrix; 
    
    // don't forget to pad to 4 floats (if needed)
};

/** Describes Material's constants. */
cbuffer materialData : register(b3, space5)
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
    // Normals may be unnormalized after the rasterization (when they are interpolated).
    float3 pixelNormalUnit = normalize(pin.worldNormal);

    // Set initial (unlit) color.
    float4 outputColor = float4(0.0F, 0.0F, 0.0F, 1.0F);

    // Prepare diffuse color.
    float3 pixelDiffuseColor = diffuseColor;
#ifdef PS_USE_DIFFUSE_TEXTURE
    float4 diffuseTextureSample = diffuseTexture.Sample(textureSampler, pin.uv);
    pixelDiffuseColor *= diffuseTextureSample.rgb;
#endif

    // Calculate light from point lights.
    for (uint i = 0; i < iPointLightCount; i++){
        outputColor.rgb += float3(pointLights[i].intensity, pointLights[i].intensity, pointLights[i].intensity); // testing code
    }

    // Apply ambient light.
    outputColor.rgb += ambientLight * pixelDiffuseColor;

#ifdef PS_USE_MATERIAL_TRANSPARENCY
    // Apply transparency.
#ifdef PS_USE_DIFFUSE_TEXTURE
    outputColor.a = diffuseTextureSample.a;
#endif
    outputColor.a *= opacity;
#endif

    return outputColor;
}