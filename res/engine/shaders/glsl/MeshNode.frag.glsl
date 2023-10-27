#include "../Base.glsl"
#include "../Lighting.glsl"
#include "../MaterialData.glsl"
#include "MeshNodePushConstants.glsl"

/** Input parameters. */
layout(location = 0) in vec4 fragmentViewPosition;
layout(location = 1) in vec4 fragmentWorldPosition;
layout(location = 2) in vec3 fragmentWorldNormal;
layout(location = 3) in vec2 fragmentUv;

/** Output parameters. */
layout(location = 0) out vec4 outputColor;

#ifdef PS_USE_DIFFUSE_TEXTURE
    layout(binding = 6) uniform sampler2D diffuseTexture[]; // "bindless binding", stores all diffuse textures
#endif

/** Fragment shader. */
void fsMeshNode(){
    // Prepare a short macro to access material data.
#define MATERIAL_DATA materialData.array[arrayIndices.materialData]

    // Normals may be unnormalized after the rasterization (when they are interpolated).
    vec3 fragmentWorldNormalUnit = normalize(fragmentWorldNormal);

    // Set initial (unlit) color.
    outputColor = vec4(0.0F, 0.0F, 0.0F, 1.0F);

    // Prepare diffuse color.
    vec3 fragmentDiffuseColor = MATERIAL_DATA.diffuseColor.rgb;
#ifdef PS_USE_DIFFUSE_TEXTURE
    vec4 diffuseTextureSample = texture(diffuseTexture[arrayIndices.diffuseTexture], fragmentUv);
    fragmentDiffuseColor *= diffuseTextureSample.rgb;
#endif

    // Prepare specular color.
    vec3 fragmentSpecularColor = vec3(1.0F, 1.0F, 1.0F);

    // Prepare material shininess.
    float materialShininess = 32.0F;

    // Calculate light from point lights.
    for (uint i = 0; i < generalLightingData.iPointLightCount; i++){
        outputColor.rgb += calculateColorFromPointLight(
            pointLights.array[i],
            vec3(frameData.cameraPosition),
            vec3(fragmentWorldPosition),
            fragmentWorldNormalUnit,
            fragmentDiffuseColor,
            fragmentSpecularColor,
            materialShininess);
    }

    // Calculate light from directional lights.
    for (uint i = 0; i < generalLightingData.iDirectionalLightCount; i++){
        outputColor.rgb += calculateColorFromDirectionalLight(
            directionalLights.array[i],
            vec3(frameData.cameraPosition),
            vec3(fragmentWorldPosition),
            fragmentWorldNormalUnit,
            fragmentDiffuseColor,
            fragmentSpecularColor,
            materialShininess);
    }

    // Apply ambient light.
    outputColor.rgb += generalLightingData.ambientLight.rgb * fragmentDiffuseColor;

#ifdef PS_USE_MATERIAL_TRANSPARENCY
    // Apply opacity.
#ifdef PS_USE_DIFFUSE_TEXTURE
    outputColor.a = diffuseTextureSample.a; // Get opacity from diffuse texture.
#endif
    outputColor.a *= MATERIAL_DATA.diffuseColor.a;
#endif
}