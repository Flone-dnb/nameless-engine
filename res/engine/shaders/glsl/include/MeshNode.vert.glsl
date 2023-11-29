#include "../../include/Base.glsl"
#include "../../include/MeshData.glsl"
#include "../misc/MeshNodePushConstants.glsl"

/** Input parameters. */
layout(location = 0) in vec3 localPosition; // position in local space
layout(location = 1) in vec3 localNormal; // normal in local space
layout(location = 2) in vec2 uv;

/** Output parameters. */
layout(location = 0) out vec4 fragmentWorldPosition;
layout(location = 1) out vec3 fragmentWorldNormal;
layout(location = 2) out vec2 fragmentUv;

/** Vertex shader. */
void vsMeshNode(){
    // Prepare a short macro to access mesh data.
#define MESH_DATA meshData.array[arrayIndices.meshData]

    // Calculate world position and normal.
    fragmentWorldPosition = MESH_DATA.worldMatrix * vec4(localPosition, 1.0F);
    fragmentWorldNormal = normalize(mat3(MESH_DATA.normalMatrix) * localNormal);

    // Transform position to homogeneous clip space.
    gl_Position = frameData.viewProjectionMatrix * fragmentWorldPosition;
    
    // Copy UV.
    fragmentUv = uv;
}