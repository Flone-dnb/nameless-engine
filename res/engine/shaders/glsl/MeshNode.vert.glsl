#include "Base.glsl"

#include "MeshNodeConstants.glsl"

layout(location = 0) in vec3 localPosition; // position in local space
layout(location = 1) in vec3 localNormal; // normal in local space
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 fragmentViewPosition;
layout(location = 1) out vec4 fragmentWorldPosition;
layout(location = 2) out vec3 fragmentNormal;
layout(location = 3) out vec2 fragmentUv;

/** Describes MeshNode's constants. */
struct MeshData {
    /** Matrix that transforms vertices from mesh local space to world space. */
    mat4 worldMatrix; 
};
layout(std140, binding = 2) readonly buffer MeshDataBuffer{
    MeshData array[];
} meshData;

void vsMeshNode(){
    // Calculate world position and normal.
    fragmentWorldPosition = meshData.array[arrayIndices.meshData].worldMatrix * vec4(localPosition, 1.0F);
    fragmentNormal = normalize(mat3(meshData.array[arrayIndices.meshData].worldMatrix) * localNormal);

    // Transform position to homogeneous clip space.
    gl_Position = frameData.viewProjectionMatrix * fragmentWorldPosition;
    fragmentViewPosition = gl_Position;
    
    // Copy UV.
    fragmentUv = uv;
}