#include "../../../../engine/shaders/include/Base.glsl"
#include "../../../../engine/shaders/hlsl/formats/MeshNodeVertex.hlsl"

#additional_shader_constants{
    uint test1;
}

VertexOut main(VertexIn vertexIn) {
    VertexOut vertexOut;
    vertexOut.position = float4(constants.test1, 0.0F, 0.0F, 1.0F);
    return vertexOut;
}
