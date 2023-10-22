#include "Base.glsl"

#include "MeshNodeConstants.glsl"
#include "Lighting.glsl"

#ifdef PS_USE_DIFFUSE_TEXTURE
    layout(binding = 4) uniform sampler2D diffuseTexture[]; // "bindless binding", stores all diffuse textures
#endif

layout(location = 0) in vec4 fragmentViewPosition;
layout(location = 1) in vec4 fragmentWorldPosition;
layout(location = 2) in vec3 fragmentNormal;
layout(location = 3) in vec2 fragmentUv;

layout(location = 0) out vec4 outputColor;

/** Describes Material's constants. */
struct MaterialData{
    /** Fill color. */
    vec3 diffuseColor;

    /** Opacity (when material transparency is used). */
    float opacity;
};
layout(std140, binding = 5) readonly buffer MaterialDataBuffer{
    MaterialData array[];
} materialData;

void fsMeshNode(){
    // Normals may be unnormalized after the rasterization (when they are interpolated).
    vec3 fragmentNormalUnit = normalize(fragmentNormal);

    // Set initial (unlit) color.
    outputColor = vec4(0.0F, 0.0F, 0.0F, 1.0F);

    // Prepare diffuse color.
    vec3 fragmentDiffuseColor = materialData.array[arrayIndices.materialData].diffuseColor;
#ifdef PS_USE_DIFFUSE_TEXTURE
    vec4 diffuseTextureSample = texture(diffuseTexture[arrayIndices.diffuseTexture], fragmentUv);
    fragmentDiffuseColor *= diffuseTextureSample.rgb;
#endif

    // Calculate light from point lights.
    for (uint i = 0; i < generalLightingData.iPointLightCount; i++){
        outputColor.rgb += vec3(pointLights.array[i].intensity); // testing code
    }

    // Apply ambient light.
    outputColor.rgb += generalLightingData.ambientLight * fragmentDiffuseColor;

#ifdef PS_USE_MATERIAL_TRANSPARENCY
    // Apply opacity.
#ifdef PS_USE_DIFFUSE_TEXTURE
    outputColor.a = diffuseTextureSample.a;
#endif
    outputColor.a *= materialData.array[arrayIndices.materialData].opacity;
#endif
}