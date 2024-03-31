/** Describes MeshNode's constants. */
struct MeshData {
    /** Matrix that transforms positions from model space to world space. */
    mat4 worldMatrix; 

    /** 3x3 matrix (using 4x4 for shader alignment/packing simpicity) that transforms normals from model space to world space. */ 
    mat4 normalMatrix;
};

#hlsl ConstantBuffer<MeshData> meshData : register(b2, space5);

#glsl{
layout(std140, binding = 5) readonly buffer MeshDataBuffer{
    MeshData array[];
} meshData;
}