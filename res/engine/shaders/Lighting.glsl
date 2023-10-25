/// Include this file if you need to have information about spawned light sources and/or general lighting.

/** Stores general lighting related data. */
#glsl layout(binding = 1) uniform GeneralLightingData{
#hlsl struct GeneralLightingData{
    /** Light color intensity of ambient lighting. 4th component is not used. */
    vec4 ambientLight;

    /** Total number of spawned point lights. */
    uint iPointLightCount;
#glsl } generalLightingData;
#hlsl }; ConstantBuffer<GeneralLightingData> generalLightingData : register(b1, space5);

/** Point light parameters. */
struct PointLight{
    /** Light position in world space. 4th component is not used. */
    vec4 position;

    /** Color of the light source. 4th component is not used. */
    vec4 color;

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

/**
 * Calculates attenuation factor for a point light source.
 *
 * @param distanceToLightSource Distance between pixel/fragment and the light source.
 * @param lightSource           Light source data.
 *
 * @return Factor in range [0.0; 1.0] where 0.0 means "no light is received" and 1.0 means "full light is received".
 */
float calculatePointLightAttenuation(float distanceToLightSource, PointLight lightSource){
    float distanceToLightDivHalfRadius = distanceToLightSource / lightSource.halfDistance;
    return lightSource.intensity / (1.0F + distanceToLightDivHalfRadius * distanceToLightDivHalfRadius);
}

/**
 * Calculates light that a pixel/fragment receives from point light source.
 *
 * @param lightSource           Light source data.
 * @param cameraPosition        Camera position in world space.
 * @param fragmentPosition      Position of the pixel/fragment in world space.
 * @param fragmentNormalUnit    Pixel/fragment's unit normal vector (interpolated vertex normal or normal from normal texture).
 * @param fragmentDiffuseColor  Pixel/fragment's diffuse color (interpolated vertex color or color from diffuse texture).
 * @param fragmentSpecularColor Pixel/fragment's specular color (interpolated vertex specular or specular from texture).
 * @param materialShininess     Shininess parameter of pixel/fragment's material.
 *
 * @return Color that pixel/fragment receives from the specified light source.
 */
vec3 calculateColorFromPointLight(
    PointLight lightSource,
    vec3 cameraPosition,
    vec3 fragmentPosition,
    vec3 fragmentNormalUnit,
    vec3 fragmentDiffuseColor,
    vec3 fragmentSpecularColor,
    float materialShininess){
    // Calculate light attenuation.
    float fragmentDistanceToLight = length(lightSource.position.xyz - fragmentPosition);
    vec3 attenuatedLightColor = lightSource.color.rgb * calculatePointLightAttenuation(fragmentDistanceToLight, lightSource);

    // Calculate diffuse color.
    vec3 fragmentToLightDirectionUnit = normalize(lightSource.position.xyz - fragmentPosition);
    float cosFragmentToLight = max(dot(fragmentNormalUnit, fragmentToLightDirectionUnit), 0.0F);
    vec3 diffuseLight = cosFragmentToLight * fragmentDiffuseColor * attenuatedLightColor;

    // Calculate specular color.
    vec3 fragmentToCameraDirectionUnit = normalize(cameraPosition - fragmentPosition);
    vec3 fragmentLightHalfwayDirectionUnit = normalize(fragmentToLightDirectionUnit + fragmentToCameraDirectionUnit);
    float specularFactor = pow(max(dot(fragmentNormalUnit, fragmentLightHalfwayDirectionUnit), 0.0), materialShininess);
    vec3 specularColor = attenuatedLightColor * (specularFactor * fragmentSpecularColor);

    return diffuseLight + specularColor;
}