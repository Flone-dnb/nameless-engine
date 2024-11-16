// NOLINTBEGIN: wrong variable case (intentional, we need variable name to match struct variable name)
#additional_shader_constants {
    uint meshData;            
    uint materialData;
    #ifdef FS_USE_DIFFUSE_TEXTURE
        uint diffuseTextures;
    #endif
}
// NOLINTEND