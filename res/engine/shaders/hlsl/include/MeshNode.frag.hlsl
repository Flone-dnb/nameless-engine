#include "../../include/Base.glsl"
#include "../../include/MaterialData.glsl"

#define INCLUDE_LIGHTING_FUNCTIONS
#define READ_ONLY_LIGHT_GRID
#include "../../include/Lighting.glsl"

#ifdef PS_USE_DIFFUSE_TEXTURE
SamplerState textureSampler : register(s0, space5);
Texture2D diffuseTexture : register(t4, space5);
#endif

/** Pixel shader. */
[earlydepthstencil]
float4 psMeshNode(VertexOut pin) {
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
        frameData.cameraPosition.xyz,
        pin.worldPosition.xyz,
        pin.position.xy,
        pixelNormalUnit,
        pixelDiffuseColor,
        pixelSpecularColor,
        materialRoughness);
    
    return outputColor;
}