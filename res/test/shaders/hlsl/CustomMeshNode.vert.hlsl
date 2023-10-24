#include "../../../engine/shaders/hlsl/MeshNode.vert.hlsl"

VertexOut vsCustomMeshNode(VertexIn vertexIn)
{
    return vsMeshNode(vertexIn);
}