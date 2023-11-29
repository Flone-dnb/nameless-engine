#include "../../../engine/shaders/hlsl/include/MeshNode.frag.hlsl"

float4 psCustomMeshNode(VertexOut pin) : SV_Target
{
    return psMeshNode(pin);
}