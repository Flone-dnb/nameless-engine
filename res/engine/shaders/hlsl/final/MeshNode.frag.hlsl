#include "../include/MeshNode.frag.hlsl"

[earlydepthstencil]
/**
 * This function is not expected be called from other shader files.
 *
 * @param pin Input fragment.
 *
 * @return Pixel color.
 */
float4 main(VertexOut pin) : SV_Target {
    return psMeshNode(pin);
}