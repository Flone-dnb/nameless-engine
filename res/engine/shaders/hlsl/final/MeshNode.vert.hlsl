#include "../include/MeshNode.vert.hlsl"

VertexOut main(VertexIn vertexIn)
{
    return vsMeshNode(vertexIn);
}
