#include "Base.glsl"

layout(location = 0) in vec3 localPosition; // position in local space
layout(location = 1) in vec3 localNormal; // normal in local space
layout(location = 2) in vec2 uv;

layout(location = 0) out vec4 fragmentViewPosition;
layout(location = 1) out vec4 fragmentWorldPosition;
layout(location = 2) out vec3 fragmentNormal;
layout(location = 3) out vec2 fragmentUv;

/** Describes MeshNode's constants. */
layout(binding = 1) uniform MeshData {
    /** Matrix that transforms vertices from mesh local space to world space. */
    mat4 worldMatrix; 
} meshData;

void main(){
    gl_Position = meshData.worldMatrix * frameData.viewProjectionMatrix * vec4(localPosition, 1.0F);

    fragmentViewPosition = gl_Position;
    fragmentWorldPosition = meshData.worldMatrix * vec4(localPosition, 1.0F);
    fragmentNormal = normalize(mat3(meshData.worldMatrix) * localNormal);
    fragmentUv = uv;
}