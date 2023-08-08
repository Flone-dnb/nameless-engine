#include "Base.glsl"

#ifdef USE_DIFFUSE_TEXTURE
    layout(binding = 2) uniform sampler2D textureSampler;
#endif

layout(location = 0) in vec4 fragmentViewPosition;
layout(location = 1) in vec4 fragmentWorldPosition;
layout(location = 2) in vec3 fragmentNormal;
layout(location = 3) in vec2 fragmentUv;

layout(location = 0) out vec4 outColor;

/** Describes indices into various arrays. */
layout(push_constant) uniform MeshIndices
{
	uint iMeshDataIndex;
    uint iMaterialIndex;
} meshIndices;

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
    outColor = vec4(materialData.array[meshIndices.iMaterialIndex].diffuseColor, 1.0F);

#ifdef USE_DIFFUSE_TEXTURE
    outColor *= texture(textureSampler, fragmentUv);
#endif
}