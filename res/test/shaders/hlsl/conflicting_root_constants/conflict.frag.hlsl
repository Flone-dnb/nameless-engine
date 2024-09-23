#include "../../../../engine/shaders/include/Base.glsl"
#include "../../../../engine/shaders/hlsl/formats/MeshNodeVertex.hlsl"

#additional_shader_constants{
    uint test42;
}

float4 main(VertexOut pin) : SV_Target {
    return float4(constants.test42, 1.0F, 1.0F, 1.0F);
}