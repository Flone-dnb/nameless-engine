#include "../Base.glsl"

/** Describes MeshNode's constants. */
struct MeshData
{
    /** Matrix that transforms vertices from mesh local space to world space. */
    float4x4 worldMatrix; 
};
ConstantBuffer<MeshData> meshData : register(b2, space5);

/** Vertex shader. */
VertexOut vsMeshNode(VertexIn vertexIn)
{
    VertexOut vertexOut;

    // Calculate world position and normal.
    vertexOut.worldPosition = mul(meshData.worldMatrix, float4(vertexIn.localPosition, 1.0F));
    vertexOut.worldNormal = normalize(mul((float3x3)meshData.worldMatrix, vertexIn.localNormal));

    // Transform position to homogeneous clip space.
    vertexOut.viewPosition = mul(frameData.viewProjectionMatrix, vertexOut.worldPosition);

    // Copy UV.
    vertexOut.uv = vertexIn.uv;

    return vertexOut;
}
