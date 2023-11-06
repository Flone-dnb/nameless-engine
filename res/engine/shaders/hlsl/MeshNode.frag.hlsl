#include "../Base.glsl"
#include "../Lighting.glsl"
#include "../MaterialData.glsl"

#ifdef PS_USE_DIFFUSE_TEXTURE
   SamplerState textureSampler : register(s0, space5);
#endif

#ifdef PS_USE_DIFFUSE_TEXTURE
    Texture2D diffuseTexture : register(t3, space5);
#endif

/** Pixel shader. */
float4 psMeshNode(VertexOut pin) : SV_Target
{
    // Normals may be unnormalized after the rasterization (when they are interpolated).
    float3 pixelNormalUnit = normalize(pin.worldNormal);

    // Prepare diffuse color.
    float3 pixelDiffuseColor = materialData.diffuseColor.rgb;
#ifdef PS_USE_DIFFUSE_TEXTURE
    float4 diffuseTextureSample = diffuseTexture.Sample(textureSampler, pin.uv);
    pixelDiffuseColor *= diffuseTextureSample.rgb;
#endif

    // Prepare specular color.
    float3 pixelSpecularColor = materialData.specularColor.rgb;

    // Prepare material roughness.
    float materialRoughness = materialData.roughness;

    // Set initial (unlit) color.
    float4 outputColor = float4(0.0F, 0.0F, 0.0F, 1.0F);

    // Calculate light.
    outputColor.rgb += calculateColorFromLights(
        (float3)frameData.cameraPosition,
        (float3)pin.worldPosition,
        pixelNormalUnit,
        pixelDiffuseColor,
        pixelSpecularColor,
        materialRoughness);

#ifdef PS_USE_MATERIAL_TRANSPARENCY
    // Apply transparency.
#ifdef PS_USE_DIFFUSE_TEXTURE
    outputColor.a = diffuseTextureSample.a;
#endif
    outputColor.a *= materialData.diffuseColor.a;
#endif

    return outputColor;
}