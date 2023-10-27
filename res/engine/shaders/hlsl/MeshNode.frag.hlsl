#include "../Base.glsl"
#include "../Lighting.glsl"
#include "../MaterialData.glsl"

#ifdef PS_USE_DIFFUSE_TEXTURE
   SamplerState textureSampler : register(s0, space5);
#endif

#ifdef PS_USE_DIFFUSE_TEXTURE
    Texture2D diffuseTexture : register(t2, space5);
#endif

/** Pixel shader. */
float4 psMeshNode(VertexOut pin) : SV_Target
{
    // Normals may be unnormalized after the rasterization (when they are interpolated).
    float3 pixelNormalUnit = normalize(pin.worldNormal);

    // Set initial (unlit) color.
    float4 outputColor = float4(0.0F, 0.0F, 0.0F, 1.0F);

    // Prepare diffuse color.
    float3 pixelDiffuseColor = materialData.diffuseColor.rgb;
#ifdef PS_USE_DIFFUSE_TEXTURE
    float4 diffuseTextureSample = diffuseTexture.Sample(textureSampler, pin.uv);
    pixelDiffuseColor *= diffuseTextureSample.rgb;
#endif

    // Prepare specular color.
    float3 pixelSpecularColor = float3(1.0F, 1.0F, 1.0F);

    // Prepare material shininess.
    float materialShininess = 32.0F;

    // Calculate light from point lights.
    for (uint i = 0; i < generalLightingData.iPointLightCount; i++){
        outputColor.rgb += calculateColorFromPointLight(
            pointLights[i],
            (float3)frameData.cameraPosition,
            (float3)pin.worldPosition,
            pixelNormalUnit,
            pixelDiffuseColor,
            pixelSpecularColor,
            materialShininess);
    }

    // Calculate light from directional lights.
    for (uint i = 0; i < generalLightingData.iDirectionalLightCount; i++){
        outputColor.rgb += calculateColorFromDirectionalLight(
            directionalLights[i],
            (float3)frameData.cameraPosition,
            (float3)pin.worldPosition,
            pixelNormalUnit,
            pixelDiffuseColor,
            pixelSpecularColor,
            materialShininess);
    }

    // Apply ambient light.
    outputColor.rgb += generalLightingData.ambientLight.rgb * pixelDiffuseColor;

#ifdef PS_USE_MATERIAL_TRANSPARENCY
    // Apply transparency.
#ifdef PS_USE_DIFFUSE_TEXTURE
    outputColor.a = diffuseTextureSample.a;
#endif
    outputColor.a *= materialData.diffuseColor.a;
#endif

    return outputColor;
}