#include "../../../engine/shaders/hlsl/include/MeshNode.vert.hlsl"

VertexOut vsCustomMeshNode(VertexIn vertexIn)
{
    return vsMeshNode(vertexIn);
}