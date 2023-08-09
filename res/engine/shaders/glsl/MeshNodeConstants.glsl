/** Describes indices into various arrays. */
layout(push_constant) uniform MeshIndices
{
	uint meshData;
    uint materialData;
} arrayIndices;