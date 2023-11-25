#include "../include/Base.glsl"
#include "../include/MaterialData.glsl"
#include "MeshNodePushConstants.glsl"

#define INCLUDE_LIGHTING_FUNCTIONS
#include "../include/Lighting.glsl"

/** Input parameters. */
layout(location = 0) in vec4 fragmentWorldPosition;
layout(location = 1) in vec3 fragmentWorldNormal;
layout(location = 2) in vec2 fragmentUv;

/** Output parameters. */
layout(location = 0) out vec4 outputColor;

#ifdef PS_USE_DIFFUSE_TEXTURE
    layout(binding = 7) uniform sampler2D diffuseTexture[]; // "bindless binding", stores all diffuse textures
#endif

/** Fragment shader. */
layout(early_fragment_tests) in;
void fsMeshNode(){
    // Prepare a short macro to access material data.
#define MATERIAL_DATA materialData.array[arrayIndices.materialData]

    // Normals may be unnormalized after the rasterization (when they are interpolated).
    vec3 fragmentWorldNormalUnit = normalize(fragmentWorldNormal);

    // Prepare diffuse color.
    vec3 fragmentDiffuseColor = MATERIAL_DATA.diffuseColor.rgb;
#ifdef PS_USE_DIFFUSE_TEXTURE
    vec4 diffuseTextureSample = texture(diffuseTexture[arrayIndices.diffuseTexture], fragmentUv);
    fragmentDiffuseColor *= diffuseTextureSample.rgb;
#endif

    // Prepare specular color.
    vec3 fragmentSpecularColor = MATERIAL_DATA.specularColor.rgb;

    // Prepare material roughness.
    float materialRoughness = MATERIAL_DATA.roughness; 

    // Set initial (unlit) color.
    outputColor = vec4(0.0F, 0.0F, 0.0F, 1.0F);

    // Calculate light.
    outputColor.rgb += calculateColorFromLights(
        frameData.cameraPosition.xyz,
        fragmentWorldPosition.xyz,
        gl_FragCoord.xy,
        fragmentWorldNormalUnit,
        fragmentDiffuseColor,
        fragmentSpecularColor,
        materialRoughness);

#ifdef PS_USE_MATERIAL_TRANSPARENCY
    // Apply opacity.
#ifdef PS_USE_DIFFUSE_TEXTURE
    outputColor.a = diffuseTextureSample.a; // Get opacity from diffuse texture.
#endif
    outputColor.a *= MATERIAL_DATA.diffuseColor.a;
#endif
}