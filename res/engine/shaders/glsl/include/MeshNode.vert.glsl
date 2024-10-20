#include "../../include/Base.glsl"
#include "../../include/data/MeshData.glsl"
#include "../../include/constants/MeshNodeConstants.glsl"
#ifdef VS_SHADOW_MAPPING_PASS
    #include "../../include/data/ShadowPassLightInfoData.glsl"
    #include "../../include/constants/ShadowPassConstants.glsl"
#endif

#define LAYOUT_VERTEX_SHADER
#include "../format/MeshNodeVertexLayout.glsl"

/** Vertex shader. */
void vsMeshNode() {
    // Prepare a short macro to access mesh data.
    #define MESH_DATA meshData.array[constants.meshData]

    // Calculate world coordinates.
    fragmentWorldPosition = MESH_DATA.worldMatrix * vec4(localPosition, 1.0F);
    #ifndef VS_SHADOW_MAPPING_PASS
        fragmentWorldNormal = normalize(mat3(MESH_DATA.normalMatrix) * localNormal);
    #endif

    // Transform position to homogeneous clip space.
    #ifdef VS_SHADOW_MAPPING_PASS
        gl_Position = shadowPassLightInfo.array[constants.iShadowPassLightInfoIndex].viewProjectionMatrix * fragmentWorldPosition;
    #else
        gl_Position = frameData.viewProjectionMatrix * fragmentWorldPosition;
    #endif

    // Copy UV.
    fragmentUv = uv;
}