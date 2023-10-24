/// Include this file if you need to have information about spawned light sources and/or general lighting.

/** Stores general lighting related data. */
struct GeneralLightingData{
    /** Light color intensity of ambient lighting. */
    float3 ambientLight;

    /** Total number of spawned point lights. */
    uint iPointLightCount;
};
ConstantBuffer<GeneralLightingData> generalLightingData : register(b1, space5);

/** Point light parameters. */
struct PointLight{
    /** Light position in world space. */
    float3 position;

    /** Color of the light source. */
    float3 color;

    /** Light intensity, valid values range is [0.0F; 1.0F]. */
    float intensity;

    /**
     * Distance where the light intensity is half the maximal intensity,
     * valid values range is [0.01F; +inf].
     */
    float halfDistance;

    // don't forget to pad to 4 floats (if needed)
};

/** All spawned point lights. */
StructuredBuffer<PointLight> pointLights : register(t0, space5);
