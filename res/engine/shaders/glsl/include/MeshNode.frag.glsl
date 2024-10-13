#include "../../include/Base.glsl"
#include "../../include/data/MaterialData.glsl"
#include "../../include/constants/MeshNodeConstants.glsl"

#define INCLUDE_LIGHTING_FUNCTIONS
#define READ_ONLY_LIGHT_GRID
#include "../../include/Lighting.glsl"

#define LAYOUT_FRAGMENT_SHADER
#include "../formats/MeshNodeVertexLayout.glsl"

/** Output parameters. */
layout(location = 0) out vec4 outputColor;

#ifdef PS_USE_DIFFUSE_TEXTURE
    layout(binding = 7) uniform sampler2D diffuseTextures[]; // "bindless binding", stores all diffuse textures
#endif

layout(early_fragment_tests) in;

/** Fragment shader. */
void fsMeshNode() {
    // Prepare a short macro to access material data.
    #define MATERIAL_DATA materialData.array[constants.materialData]
    
    // Normals may be unnormalized after the rasterization (when they are interpolated).
    vec3 fragmentWorldNormalUnit = normalize(fragmentWorldNormal);
    
    // Prepare diffuse color.
    vec3 fragmentDiffuseColor = MATERIAL_DATA.diffuseColor.rgb;
    #ifdef PS_USE_DIFFUSE_TEXTURE
        vec4 diffuseTextureSample = texture(diffuseTextures[constants.diffuseTextures], fragmentUv);
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
            // Get opacity from diffuse texture.
            outputColor.a = diffuseTextureSample.a;
        #endif
        outputColor.a *= MATERIAL_DATA.diffuseColor.a;
    #endif
}