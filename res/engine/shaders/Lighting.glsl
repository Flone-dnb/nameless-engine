/// Include this file if you need to have information about spawned light sources and/or general lighting.

/** Stores general lighting related data. */
#glsl layout(binding = 1) uniform GeneralLightingData{
#hlsl struct GeneralLightingData{
    /** Light color intensity of ambient lighting. */
    vec3 ambientLight;

    /** Total number of spawned point lights. */
    uint iPointLightCount;
#glsl } generalLightingData;
#hlsl }; ConstantBuffer<GeneralLightingData> generalLightingData : register(b1, space5);

/** Point light parameters. */
struct PointLight{
    /** Light position in world space. */
    vec3 position;

    /** Color of the light source. */
    vec3 color;

    /** Light intensity, valid values range is [0.0F; 1.0F]. */
    float intensity;

    /**
     * Distance where the light intensity is half the maximal intensity,
     * valid values range is [0.01F; +inf].
     */
    float halfDistance;
};

/** All spawned point lights. */
#glsl{
layout(std140, binding = 2) readonly buffer PointLightsBuffer{
    PointLight array[];
} pointLights;
}
#hlsl StructuredBuffer<PointLight> pointLights : register(t0, space5);