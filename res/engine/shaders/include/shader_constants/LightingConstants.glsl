/** Specifies which `viewProjectionMatrix` to use from the array of matrices. */
#additional_shader_constants{
#ifdef VS_SHADOW_MAPPING_PASS
uint iLightViewProjectionMatrixIndex;
#endif
}