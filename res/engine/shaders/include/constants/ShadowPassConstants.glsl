#additional_shader_constants {
    #if defined(VS_SHADOW_MAPPING_PASS) || defined(POINT_LIGHT_SHADOW_PASS)
        uint iShadowPassLightInfoIndex;
    #endif
}