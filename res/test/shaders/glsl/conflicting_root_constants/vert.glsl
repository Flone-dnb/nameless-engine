#include "../../../../engine/shaders/include/Base.glsl"
#define LAYOUT_VERTEX_SHADER
#include "../../../../engine/shaders/glsl/format/MeshNodeVertexLayout.glsl"

#additional_shader_constants{
    uint test1;
}

void main() {
    gl_Position = vec4(constants.test1, 0.0F, 0.0F, 1.0F);
}
