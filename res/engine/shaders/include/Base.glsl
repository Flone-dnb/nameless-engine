#glsl {
    #version 460
    #extension GL_ARB_separate_shader_objects : enable
    #extension GL_EXT_nonuniform_qualifier : enable
}

#include "data/FrameData.glsl"

/** Initial (empty) push/root constants definition in order for later `#additional_..._constants` to work. */
#glsl {
    layout(push_constant) uniform PushConstants {
    } constants;
}
#hlsl {
    struct RootConstants {
    }; ConstantBuffer<RootConstants> constants : register(b0, space8);
}