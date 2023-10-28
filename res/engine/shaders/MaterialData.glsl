// Include this file if you need to have information about material constants.

/** Describes Material's constants. */
struct MaterialData{
    /** Fill color. 4th component stores opacity (when material transparency is used). */
    vec4 diffuseColor;

    /** Reflected color. 4th component is not used. */
    vec4 specularColor;

    /** Defines how much specular light will be reflected (value in range [0.0F; 1.0F]). */
    float roughness;
};

#hlsl ConstantBuffer<MaterialData> materialData : register(b3, space5);

#glsl{
layout(std140, binding = 5) readonly buffer MaterialDataBuffer{
    MaterialData array[];
} materialData;
}