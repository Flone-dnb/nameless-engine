/// Include this file if you need to have information about spawned light sources and/or general lighting.

/** Stores general lighting related data. */
layout(binding = 1) uniform GeneralLightingData{
    /** Light color intensity of ambient lighting. */
    vec3 ambientLight;

    /** Total number of spawned point lights. */
    uint iPointLightCount;
} generalLightingData;

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
layout(std140, binding = 2) readonly buffer PointLightsBuffer{
    PointLight array[];
} pointLights;