#include "../../../../engine/shaders/include/Base.glsl"
#define LAYOUT_FRAGMENT_SHADER
#include "../../../../engine/shaders/glsl/formats/MeshNodeVertexLayout.glsl"

#additional_shader_constants{
    uint test1;
}

layout(location = 0) out vec4 outputColor;

void main() {
    outputColor = vec4(constants.test1, 1.0F, 1.0F, 1.0F);
}