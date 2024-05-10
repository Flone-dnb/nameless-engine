// NOLINTBEGIN: wrong variable case (intentional)
#additional_push_constants {
    uint meshData;            
    uint materialData;
    #ifdef PS_USE_DIFFUSE_TEXTURE
        uint diffuseTexture;
    #endif
}
// NOLINTEND