#include "Base.glsl"

#include "MeshNodeConstants.glsl"

#ifdef PS_USE_DIFFUSE_TEXTURE
    layout(binding = 1) uniform sampler2D diffuseTexture[];
#endif

layout(location = 0) in vec4 fragmentViewPosition;
layout(location = 1) in vec4 fragmentWorldPosition;
layout(location = 2) in vec3 fragmentNormal;
layout(location = 3) in vec2 fragmentUv;

layout(location = 0) out vec4 outputColor;

/** Describes Material's constants. */
struct MaterialData
{
    /** Fill color. */
    vec3 diffuseColor;

    /** Opacity (when material transparency is used). */
    float opacity;
};
layout(std140, binding = 3) readonly buffer MaterialDataBuffer{
    MaterialData array[];
} materialData;

void fsMeshNode(){
    // Set diffuse color.
    outputColor = vec4(materialData.array[arrayIndices.materialData].diffuseColor, 1.0F);

#ifdef PS_USE_DIFFUSE_TEXTURE
    // Apply diffuse texture.
    outputColor *= texture(diffuseTexture[arrayIndices.diffuseTexture], fragmentUv);
#endif

    // Apply ambient light.
    outputColor.rgb *= frameData.ambientLight;

#ifdef PS_USE_MATERIAL_TRANSPARENCY
    // Apply opacity.
    outputColor.a *= materialData.array[arrayIndices.materialData].opacity;
#endif
}