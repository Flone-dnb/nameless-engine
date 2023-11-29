#include "../include/MeshNode.frag.hlsl"

[earlydepthstencil]
float4 main(VertexOut pin) : SV_Target
{
    return psMeshNode(pin);
}