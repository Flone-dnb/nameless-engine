/** Describes Material's constants. */
struct MaterialData {
    /** Fill color. 4th component stores opacity (when material transparency is used). */
    vec4 diffuseColor;
    
    /** Reflected color. 4th component is not used. */
    vec4 specularColor;
    
    /** Defines how much specular light will be reflected (value in range [0.0F; 1.0F]). */
    float roughness;
    
    /** Explicit padding for clarity. */
    /** `vec3` does not work here because GLSL requires 16 bytes of alignment of `vec3`s but we have 4 byte alignment here. */
    float pad1;
    // silence doc checker
    float pad2;
    // silence doc checker
    float pad3;
};

/** Material data for all meshes, use indices from root/push constants to index into this array. */
#glsl {
    layout(std140, binding = 6) readonly buffer MaterialDataBuffer {
        /** Material data for a mesh. */
        MaterialData array[];
    } materialData;
}
#hlsl StructuredBuffer<MaterialData> materialData : register(t1, space5);