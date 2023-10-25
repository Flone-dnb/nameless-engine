#include "../Base.glsl"

/** Describes MeshNode's constants. */
struct MeshData
{
    /** Matrix that transforms positions from model space to world space. */
    float4x4 worldMatrix; 

    /** 3x3 matrix (using 4x4 for shader alignment/packing simpicity) that transforms normals from model space to world space. */ 
    float4x4 normalMatrix;
};
ConstantBuffer<MeshData> meshData : register(b2, space5);

/** Vertex shader. */
VertexOut vsMeshNode(VertexIn vertexIn)
{
    VertexOut vertexOut;

    // Calculate world position and normal.
    vertexOut.worldPosition = mul(meshData.worldMatrix, float4(vertexIn.localPosition, 1.0F));
    vertexOut.worldNormal = normalize(mul((float3x3)meshData.normalMatrix, vertexIn.localNormal));

    // Transform position to homogeneous clip space.
    vertexOut.viewPosition = mul(frameData.viewProjectionMatrix, vertexOut.worldPosition);

    // Copy UV.
    vertexOut.uv = vertexIn.uv;

    return vertexOut;
}
