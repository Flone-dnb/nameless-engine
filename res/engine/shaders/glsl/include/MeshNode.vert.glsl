#include "../../include/Base.glsl"
#include "../../include/MeshData.glsl"
#include "../../include/shader_constants/MeshNodeShaderConstants.glsl"
#ifdef VS_SHADOW_MAPPING_PASS
#include "../../include/Lighting.glsl"
#include "../../include/shader_constants/LightingConstants.glsl"
#endif

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
#define MESH_DATA meshData.array[constants.meshData]

    // Calculate world coordinates.
    fragmentWorldPosition = MESH_DATA.worldMatrix * vec4(localPosition, 1.0F);
    fragmentWorldNormal = normalize(mat3(MESH_DATA.normalMatrix) * localNormal);

    // Transform position to homogeneous clip space.
#ifdef VS_SHADOW_MAPPING_PASS
    gl_Position = lightViewProjectionMatrices.array[constants.iLightViewProjectionMatrixIndex] * fragmentWorldPosition;
#else
    gl_Position = frameData.viewProjectionMatrix * fragmentWorldPosition;
#endif
    
    // Copy UV.
    fragmentUv = uv;
}