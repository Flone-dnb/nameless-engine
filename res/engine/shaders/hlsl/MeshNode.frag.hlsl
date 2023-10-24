#include "../Base.glsl"
#include "../Lighting.glsl"

#ifdef PS_USE_DIFFUSE_TEXTURE
   SamplerState textureSampler : register(s0, space5);
#endif

#ifdef PS_USE_DIFFUSE_TEXTURE
    Texture2D diffuseTexture : register(t1, space5);
#endif

/** Describes Material's constants. */
struct MaterialData
{
    /** Fill color. */
    float3 diffuseColor;

    /** Opacity (when material transparency is used). */
    float opacity;
};
ConstantBuffer<MaterialData> materialData : register(b3, space5);

/** Pixel shader. */
float4 psMeshNode(VertexOut pin) : SV_Target
{
    // Normals may be unnormalized after the rasterization (when they are interpolated).
    float3 pixelNormalUnit = normalize(pin.worldNormal);

    // Set initial (unlit) color.
    float4 outputColor = float4(0.0F, 0.0F, 0.0F, 1.0F);

    // Prepare diffuse color.
    float3 pixelDiffuseColor = materialData.diffuseColor;
#ifdef PS_USE_DIFFUSE_TEXTURE
    float4 diffuseTextureSample = diffuseTexture.Sample(textureSampler, pin.uv);
    pixelDiffuseColor *= diffuseTextureSample.rgb;
#endif

    // Calculate light from point lights.
    for (uint i = 0; i < generalLightingData.iPointLightCount; i++){
        outputColor.rgb += float3(pointLights[i].intensity, pointLights[i].intensity, pointLights[i].intensity); // testing code
    }

    // Apply ambient light.
    outputColor.rgb += generalLightingData.ambientLight * pixelDiffuseColor;

#ifdef PS_USE_MATERIAL_TRANSPARENCY
    // Apply transparency.
#ifdef PS_USE_DIFFUSE_TEXTURE
    outputColor.a = diffuseTextureSample.a;
#endif
    outputColor.a *= materialData.opacity;
#endif

    return outputColor;
}