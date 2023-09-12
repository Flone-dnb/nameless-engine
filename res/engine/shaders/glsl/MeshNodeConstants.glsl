/** Describes indices into various arrays. */
layout(push_constant) uniform MeshIndices
{
	uint meshData;
    uint materialData;
#ifdef PS_USE_DIFFUSE_TEXTURE
    uint diffuseTextures;
#endif
} arrayIndices;