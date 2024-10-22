#include "../../../../engine/shaders/include/Base.glsl"
#define LAYOUT_FRAGMENT_SHADER
#include "../../../../engine/shaders/glsl/format/MeshNodeVertexLayout.glsl"

#additional_shader_constants{
    uint test42;
}

layout(location = 0) out vec4 outputColor;

void main() {
    outputColor = vec4(constants.test42, 1.0F, 1.0F, 1.0F);
}