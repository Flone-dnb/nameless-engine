// Make sure shader type macro is defined.
#if !defined(LAYOUT_VERTEX_SHADER) && !defined(LAYOUT_FRAGMENT_SHADER)
    // Expected a layout macro to be defined.
    LAYOUT_MACRO_EXPECTED;
#endif

#ifdef LAYOUT_VERTEX_SHADER
    layout(location = VERTEX_LAYOUT_POS_BINDING_INDEX)    in vec3 localPosition;
    layout(location = VERTEX_LAYOUT_NORMAL_BINDING_INDEX) in vec3 localNormal;
    layout(location = VERTEX_LAYOUT_UV_BINDING_INDEX)     in vec2 uv;
#endif

#ifdef LAYOUT_VERTEX_SHADER
    #define VERTEX_LAYOUT_INPUT_MODIFIER out
#else
    #define VERTEX_LAYOUT_INPUT_MODIFIER in
#endif

layout(location = VERTEX_LAYOUT_POS_BINDING_INDEX)    VERTEX_LAYOUT_INPUT_MODIFIER vec4 fragmentWorldPosition;
layout(location = VERTEX_LAYOUT_NORMAL_BINDING_INDEX) VERTEX_LAYOUT_INPUT_MODIFIER vec3 fragmentWorldNormal;
layout(location = VERTEX_LAYOUT_UV_BINDING_INDEX)     VERTEX_LAYOUT_INPUT_MODIFIER vec2 fragmentUv;