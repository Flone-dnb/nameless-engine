#include "../../../engine/shaders/hlsl/MeshNode.hlsl"

VertexOut vsCustomMeshNode(VertexIn vertexIn)
{
    return vsMeshNode(vertexIn);
}

float4 psCustomMeshNode(VertexOut pin) : SV_Target
{
    return psMeshNode(pin);
}