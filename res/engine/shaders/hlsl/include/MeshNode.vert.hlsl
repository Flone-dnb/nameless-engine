#include "../../include/Base.glsl"
#include "../../include/data/MeshData.glsl"
#include "../../include/constants/MeshNodeConstants.glsl"
#ifdef VS_SHADOW_MAPPING_PASS
    #include "../../include/data/ShadowPassLightInfoData.glsl"
    #include "../../include/constants/ShadowPassConstants.glsl"
#endif

#include "../formats/MeshNodeVertex.hlsl"

/**
 * Vertex shader.
 *
 * @param vertexIn Vertex.
 *
 * @return Processed vertex.
 */
VertexOut vsMeshNode(VertexIn vertexIn) {
    // Prepare a short macro to access mesh data.
    #define MESH_DATA meshData[constants.meshData]
    
    // Prepare output variable.
    VertexOut vertexOut;
    
    // Calculate world coordinates.
    vertexOut.worldPosition = mul(MESH_DATA.worldMatrix, float4(vertexIn.localPosition, 1.0F));
    #ifndef VS_SHADOW_MAPPING_PASS
        vertexOut.worldNormal = normalize(mul((float3x3)MESH_DATA.normalMatrix, vertexIn.localNormal));
    #endif
    
    // Transform position to homogeneous clip space.
    #ifdef VS_SHADOW_MAPPING_PASS
        vertexOut.position = mul(shadowPassLightInfo[constants.iShadowPassLightInfoIndex].viewProjectionMatrix, vertexOut.worldPosition);
    #else
        vertexOut.position = mul(frameData.viewProjectionMatrix, vertexOut.worldPosition);
    #endif
    
    // Copy UV.
    vertexOut.uv = vertexIn.uv;
    
    return vertexOut;
}
