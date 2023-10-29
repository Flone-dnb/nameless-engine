#include "../Base.glsl"
#include "../MeshData.glsl"

/** Vertex shader. */
VertexOut vsMeshNode(VertexIn vertexIn)
{
    // Prepare output variable.
    VertexOut vertexOut;

    // Calculate world position and normal.
    vertexOut.worldPosition = mul(meshData.worldMatrix, float4(vertexIn.localPosition, 1.0F));
    vertexOut.worldNormal = normalize(mul((float3x3)meshData.normalMatrix, vertexIn.localNormal));

    // Transform position to homogeneous clip space.
    vertexOut.clipSpacePosition = mul(frameData.viewProjectionMatrix, vertexOut.worldPosition);

    // Copy UV.
    vertexOut.uv = vertexIn.uv;

    return vertexOut;
}
