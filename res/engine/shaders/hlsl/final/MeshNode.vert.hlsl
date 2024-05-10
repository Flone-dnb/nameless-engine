#include "../include/MeshNode.vert.hlsl"

/**
 * This function is not expected be called from other shader files.
 *
 * @param vertexIn Input vertex.
 *
 * @return Vertex.
 */
VertexOut main(VertexIn vertexIn) {
    return vsMeshNode(vertexIn);
}
