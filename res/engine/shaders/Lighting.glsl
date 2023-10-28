/// Include this file if you need to have information about spawned light sources and/or general lighting.

/** Stores general lighting related data. */
#glsl layout(binding = 1) uniform GeneralLightingData{
#hlsl struct GeneralLightingData{
    /** Light color intensity of ambient lighting. 4th component is not used. */
    vec4 ambientLight;

    /** Total number of spawned point lights. */
    uint iPointLightCount;

    /** Total number of spawned directional lights. */
    uint iDirectionalLightCount;
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

/** Directional light parameters. */
struct DirectionalLight{
    /** Light forward unit vector (direction). 4th component is not used. */
    vec4 direction;

    /** Color of the light source. 4th component is not used. */
    vec4 color;

    /** Light intensity, valid values range is [0.0F; 1.0F]. */
    float intensity;
};

/** All spawned directional lights. */
#glsl{
layout(std140, binding = 3) readonly buffer DirectionalLightsBuffer{
    DirectionalLight array[];
} directionalLights;
}
#hlsl StructuredBuffer<DirectionalLight> directionalLights : register(t1, space5);

/**
 * Calculates light reflected to the camera (Fresnel effect).
 *
 * @param fragmentSpecularColor Pixel/fragment's specular color (interpolated vertex specular or specular from texture).
 * @param fragmentHalfwayVector Halfway vector from Blinn-Phong shading, similar to surface normal.
 * @param lightDirectionUnit    Normalized direction of the lighting (this can be direction to pixel/fragment for point lights or
 * direction of the light source for directional lights).
 *
 * @return Reflected light.
 */
vec3 calculateSpecularLight(vec3 fragmentSpecularColor, vec3 fragmentHalfwayVector, vec3 lightDirectionUnit)
{
    // Using Schlick's approximation to Fresnel effect.
    float cosIncidentLight = max(dot(fragmentHalfwayVector, -lightDirectionUnit), 0.0F);
    float oneMinusCos = 1.0F - cosIncidentLight;
    
    return fragmentSpecularColor + (1.0F - fragmentSpecularColor) * (oneMinusCos * oneMinusCos * oneMinusCos * oneMinusCos * oneMinusCos);
}

/**
 * Calculates light that a pixel/fragment receives from light source.
 *
 * @param attenuatedLightColor  Color of the light source's light with attenuation (using distance not angle) applied.
 * @param lightDirectionUnit    Normalized direction of the lighting (this can be direction to pixel/fragment for point lights or
 * direction of the light source for directional lights).
 * @param cameraPosition        Camera position in world space.
 * @param fragmentPosition      Position of the pixel/fragment in world space.
 * @param fragmentNormalUnit    Pixel/fragment's unit normal vector (interpolated vertex normal or normal from normal texture).
 * @param fragmentDiffuseColor  Pixel/fragment's diffuse color (interpolated vertex color or color from diffuse texture).
 * @param fragmentSpecularColor Pixel/fragment's specular color (interpolated vertex specular or specular from texture).
 * @param materialRoughness     Roughness parameter of pixel/fragment's material.
 *
 * @return Color that pixel/fragment receives from the specified light source.
 */
vec3 calculateColorFromLightSource(
    vec3 attenuatedLightColor,
    vec3 lightDirectionUnit,
    vec3 cameraPosition,
    vec3 fragmentPosition,
    vec3 fragmentNormalUnit,
    vec3 fragmentDiffuseColor,
    vec3 fragmentSpecularColor,
    float materialRoughness){
    // Using Blinn-Phong shading.

    // Calculate angle of incidence of the light (Lambertian reflection, Lambert's cosine law)
    // to scale down light depending on the angle.
    vec3 fragmentToLightDirectionUnit = -lightDirectionUnit;
    float cosFragmentToLight = max(dot(fragmentNormalUnit, fragmentToLightDirectionUnit), 0.0F);
    attenuatedLightColor *= cosFragmentToLight;

    // Calculate roughness factor using microfacet shading.
    vec3 fragmentToCameraDirectionUnit = normalize(cameraPosition - fragmentPosition);
    vec3 fragmentLightHalfwayDirectionUnit = normalize(fragmentToLightDirectionUnit + fragmentToCameraDirectionUnit);
    float smoothness = max((1.0F - materialRoughness) * 256.0F, 1.0F); // avoid smoothness < 1 for `pow` below
    float roughnessFactor
        = (smoothness + 8.0F) * pow(max(dot(fragmentLightHalfwayDirectionUnit, fragmentNormalUnit), 0.0F ), smoothness) / 8.0F;

    // Calculate light reflected due to Fresnel effect.
    vec3 specularLight = calculateSpecularLight(fragmentSpecularColor, fragmentLightHalfwayDirectionUnit, lightDirectionUnit);

    // Combine Fresnel effect and microfacet roughness.
    vec3 specularColor = specularLight * roughnessFactor;

    // Specular formula above goes outside [0.0; 1.0] range,
    // but because we are doing LDR rendering scale spacular color down a bit.
    specularColor = specularColor / (specularColor + 1.0F);

    return (fragmentDiffuseColor + specularColor) * attenuatedLightColor;
}

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
 * @param materialShininess     Roughness parameter of pixel/fragment's material.
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
    float materialRoughness){
    // Calculate light attenuation.
    float fragmentDistanceToLight = length(lightSource.position.xyz - fragmentPosition);
    vec3 attenuatedLightColor = lightSource.color.rgb * calculatePointLightAttenuation(fragmentDistanceToLight, lightSource);

    return calculateColorFromLightSource(
        attenuatedLightColor,
        normalize(fragmentPosition - lightSource.position.xyz),
        cameraPosition,
        fragmentPosition,
        fragmentNormalUnit,
        fragmentDiffuseColor,
        fragmentSpecularColor,
        materialRoughness);
}

/**
 * Calculates light that a pixel/fragment receives from directional light source.
 *
 * @param lightSource           Light source data.
 * @param cameraPosition        Camera position in world space.
 * @param fragmentPosition      Position of the pixel/fragment in world space.
 * @param fragmentNormalUnit    Pixel/fragment's unit normal vector (interpolated vertex normal or normal from normal texture).
 * @param fragmentDiffuseColor  Pixel/fragment's diffuse color (interpolated vertex color or color from diffuse texture).
 * @param fragmentSpecularColor Pixel/fragment's specular color (interpolated vertex specular or specular from texture).
 * @param materialRoughness     Roughness parameter of pixel/fragment's material.
 *
 * @return Color that pixel/fragment receives from the specified light source.
 */
vec3 calculateColorFromDirectionalLight(
    DirectionalLight lightSource,
    vec3 cameraPosition,
    vec3 fragmentPosition,
    vec3 fragmentNormalUnit,
    vec3 fragmentDiffuseColor,
    vec3 fragmentSpecularColor,
    float materialRoughness){
    // Calculate light attenuation.
    vec3 attenuatedLightColor = lightSource.color.rgb * lightSource.intensity;

    return calculateColorFromLightSource(
        attenuatedLightColor,
        lightSource.direction.xyz,
        cameraPosition,
        fragmentPosition,
        fragmentNormalUnit,
        fragmentDiffuseColor,
        fragmentSpecularColor,
        materialRoughness);
}