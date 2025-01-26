#include "../../include/Base.glsl"
#include "../../include/data/MaterialData.glsl"
#include "../../include/constants/MeshNodeConstants.glsl"

#additional_shader_constants {
    uint iIndexIntoTextureSamplersArrayForDiffuse;
}

#define INCLUDE_LIGHTING_FUNCTIONS
#define READ_ONLY_LIGHT_GRID
#include "../../include/Lighting.glsl"

#include "../format/MeshNodeVertex.hlsl"

/** Stores point, linear and anisotropic samplers in this particular order. */
SamplerState textureSamplers[] : register(s0, space5);

#ifdef FS_USE_DIFFUSE_TEXTURE
    Texture2D diffuseTextures[] : register(t2, space5); // "bindless binding", stores all diffuse textures
#endif

[earlydepthstencil]
/**
 * Pixel shader.
 *
 * @param pin Fragment.
 * 
 * @return Pixel color.
 */
float4 psMeshNode(VertexOut pin) {
    // Prepare a short macro to access material data.
    #define MATERIAL_DATA materialData[constants.materialData]

    // Normals may be unnormalized after the rasterization (when they are interpolated).
    float3 pixelNormalUnit = normalize(pin.worldNormal);

    // Prepare diffuse color.
    float3 pixelDiffuseColor = MATERIAL_DATA.diffuseColor.rgb;
    #ifdef FS_USE_DIFFUSE_TEXTURE
        float4 diffuseTextureSample = diffuseTextures[constants.diffuseTextures].Sample(
            textureSamplers[constants.iIndexIntoTextureSamplersArrayForDiffuse], pin.uv);
        pixelDiffuseColor *= diffuseTextureSample.rgb;
    #endif

    // Prepare specular color.
    float3 pixelSpecularColor = MATERIAL_DATA.specularColor.rgb;

    // Prepare material roughness.
    float materialRoughness = MATERIAL_DATA.roughness;

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

    #ifdef FS_USE_MATERIAL_TRANSPARENCY
        // Apply transparency.
        #ifdef FS_USE_DIFFUSE_TEXTURE
            outputColor.a = diffuseTextureSample.a;
        #endif
        outputColor.a *= MATERIAL_DATA.diffuseColor.a;
    #endif

    return outputColor;
}