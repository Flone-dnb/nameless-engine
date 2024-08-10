/** Describes MeshNode's constants. */
struct MeshData {
    /** Matrix that transforms positions from model space to world space. */
    mat4 worldMatrix; 
    
    /** 3x3 matrix (using 4x4 for shader alignment/packing simpicity) that transforms normals from model space to world space. */ 
    mat4 normalMatrix;
};

/** Mesh constants for all meshes, use indices from root/push constants to index into this array. */
#glsl {
    layout(std140, binding = 5) readonly buffer MeshDataBuffer {
        /** Constants for a mesh. */
        MeshData array[];
    } meshData;
}
#hlsl StructuredBuffer<MeshData> meshData : register(t0, space5);