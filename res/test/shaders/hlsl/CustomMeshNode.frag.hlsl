#include "../../../engine/shaders/hlsl/MeshNode.frag.hlsl"

float4 psCustomMeshNode(VertexOut pin) : SV_Target
{
    return psMeshNode(pin);
}