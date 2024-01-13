#include "../../include/Base.glsl"
#include "../../include/MeshData.glsl"
#ifdef VS_SHADOW_MAPPING_PASS
#include "../../include/ShadowPassLightInfoData.glsl"
#include "../../include/shader_constants/ShadowPassConstants.glsl"
#endif

/** Vertex shader. */
VertexOut vsMeshNode(VertexIn vertexIn)
{
    // Prepare output variable.
    VertexOut vertexOut;

    // Calculate world coordinates.
    vertexOut.worldPosition = mul(meshData.worldMatrix, float4(vertexIn.localPosition, 1.0F));
#ifndef VS_SHADOW_MAPPING_PASS
    vertexOut.worldNormal = normalize(mul((float3x3)meshData.normalMatrix, vertexIn.localNormal));
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
