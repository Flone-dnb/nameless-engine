#include "Base.glsl"

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

layout(std140, binding = 1) readonly buffer MeshDataBuffer{
    MeshData data[];
} meshDataBuffer;

void vsMeshNode(){
    gl_Position = meshDataBuffer.data[gl_BaseInstance].worldMatrix * frameData.viewProjectionMatrix * vec4(localPosition, 1.0F);

    fragmentViewPosition = gl_Position;
    fragmentWorldPosition = meshDataBuffer.data[gl_BaseInstance].worldMatrix * vec4(localPosition, 1.0F);
    fragmentNormal = normalize(mat3(meshDataBuffer.data[gl_BaseInstance].worldMatrix) * localNormal);
    fragmentUv = uv;
}