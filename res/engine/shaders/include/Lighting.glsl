/// Include this file if you need to have information about spawned light sources and/or general lighting.

#ifdef INCLUDE_LIGHTING_FUNCTIONS // define this macro before including to have pixel/fragment light calculation functions
#ifdef PS_USE_MATERIAL_TRANSPARENCY
#include "light_culling/TransparentLightGrid.glsl"
#else
#include "light_culling/OpaqueLightGrid.glsl"
#endif
#endif

/** Stores general lighting related data. */
#glsl layout(binding = 1) uniform GeneralLightingData{
#hlsl struct GeneralLightingData{
    /** Light color intensity of ambient lighting. 4th component is not used. */
    vec4 ambientLight;

    /** Total number of spawned point lights. */
    uint iPointLightCount;

    /** Total number of spawned directional lights. */
    uint iDirectionalLightCount;

    /** Total number of spawned spotlights. */
    uint iSpotlightCount;
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

    /** Lit distance. */
    float distance;

#hlsl float2 pad1; // to pack as in Vulkan
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

#hlsl float3 pad1; // to pack as in Vulkan
};

/** All spawned directional lights. */
#glsl{
layout(std140, binding = 3) readonly buffer DirectionalLightsBuffer{
    DirectionalLight array[];
} directionalLights;
}
#hlsl StructuredBuffer<DirectionalLight> directionalLights : register(t1, space5);

/** Spotlight parameters. */
struct Spotlight{
    /** Light position in world space. 4th component is not used. */
    vec4 position;

    /** Light forward unit vector (direction) in world space. 4th component is not used. */
    vec4 direction;

    /** Color of the light source. 4th component is not used. */
    vec4 color;

    /** Light intensity, valid values range is [0.0F; 1.0F]. */
    float intensity;

    /** Lit distance. */
    float distance;

    /**
     * Cosine of the spotlight's inner cone angle (cutoff).
     *
     * @remark Represents cosine of the cutoff angle on one side from the light direction
     * (not both sides), i.e. this is a cosine of value [0-90] degrees.
     */
    float cosInnerConeAngle;

    /**
     * Cosine of the spotlight's outer cone angle (cutoff).
     *
     * @remark Represents cosine of the cutoff angle on one side from the light direction
     * (not both sides), i.e. this is a cosine of value [0-90] degrees.
     */
    float cosOuterConeAngle;

    /** Radius of cone's bottom part. */
    float coneBottomRadius;
};

/** All spawned spotlights. */
#glsl{
layout(std140, binding = 4) readonly buffer SpotlightsBuffer{
    Spotlight array[];
} spotlights;
}
#hlsl StructuredBuffer<Spotlight> spotlights : register(t2, space5);

#ifdef INCLUDE_LIGHTING_FUNCTIONS

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
 * Calculates attenuation factor for a point/spot light sources.
 *
 * @param distanceToLightSource Distance between pixel/fragment and the light source.
 * @param lightIntensity        Light intensity, valid values range is [0.0F; 1.0F].
 * @param lightDistance         Lit distance.
 *
 * @return Factor in range [0.0; 1.0] where 0.0 means "no light is received" and 1.0 means "full light is received".
 */
float calculateLightAttenuation(float distanceToLightSource, float lightIntensity, float lightDistance){
    // Use linear attenuation because it allows us to do sphere/cone intersection tests in light culling
    // more "efficient" in terms of light radius/distance to lit area. For example with linear attenuation if we have lit
    // distance 30 almost all this distance will be somewhat lit while with "inverse squared distance" function (and similar)
    // almost half of this distance will have pretty much no light.
    return clamp((lightDistance - distanceToLightSource) / lightDistance, 0.0F, 1.0F) * lightIntensity;
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
 * @param materialRoughness     Roughness parameter of pixel/fragment's material.
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
    vec3 attenuatedLightColor = lightSource.color.rgb
        * calculateLightAttenuation(fragmentDistanceToLight, lightSource.intensity, lightSource.distance);

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

/**
 * Calculates light that a pixel/fragment receives from a spotlight.
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
vec3 calculateColorFromSpotlight(
    Spotlight lightSource,
    vec3 cameraPosition,
    vec3 fragmentPosition,
    vec3 fragmentNormalUnit,
    vec3 fragmentDiffuseColor,
    vec3 fragmentSpecularColor,
    float materialRoughness){
    // Calculate light attenuation.
    float fragmentDistanceToLight = length(lightSource.position.xyz - fragmentPosition);
    vec3 attenuatedLightColor = lightSource.color.rgb
        * calculateLightAttenuation(fragmentDistanceToLight, lightSource.intensity, lightSource.distance);

    // Calculate angle between light direction and direction from fragment to light source
    // to see if this fragment is inside of the light cone or not.
    vec3 fragmentToLightDirectionUnit = normalize(lightSource.position.xyz - fragmentPosition);
    float cosDirectionAngle = dot(fragmentToLightDirectionUnit, -lightSource.direction.xyz);

    // Calculate intensity based on inner/outer cone to attenuate light.
    float lightIntensity
        = clamp((cosDirectionAngle - lightSource.cosOuterConeAngle) / (lightSource.cosInnerConeAngle - lightSource.cosOuterConeAngle),
            0.0F, 1.0F);
    attenuatedLightColor *= lightIntensity;

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

/**
 * Calculates light that a pixel/fragment receives from world's light sources.
 *
 * @remark Also includes ambient light.
 *
 * @param cameraPosition              Camera position in world space.
 * @param fragmentPositionWorldSpace  Position of the pixel/fragment in world space.
 * @param fragmentPositionScreenSpace Position of the pixel/fragment in screen space.
 * @param fragmentNormalUnit          Pixel/fragment's unit normal vector (interpolated vertex normal or normal from normal texture).
 * @param fragmentDiffuseColor        Pixel/fragment's diffuse color (interpolated vertex color or color from diffuse texture).
 * @param fragmentSpecularColor       Pixel/fragment's specular color (interpolated vertex specular or specular from texture).
 * @param materialRoughness           Roughness parameter of pixel/fragment's material.
 *
 * @return Color that pixel/fragment receives from world's light sources.
 */
vec3 calculateColorFromLights(
    vec3 cameraPosition,
    vec3 fragmentPositionWorldSpace,
    vec2 fragmentPositionScreenSpace,
    vec3 fragmentNormalUnit,
    vec3 fragmentDiffuseColor,
    vec3 fragmentSpecularColor,
    float materialRoughness){
    // Prepare final color.
    vec3 outputColor = vec3(0.0F, 0.0F, 0.0F);

    // Calculate light grid tile index.
#hlsl uint2 tileIndex = uint2(
#glsl ivec2 tileIndex = ivec2(
        floor(fragmentPositionScreenSpace / LIGHT_GRID_TILE_SIZE_IN_PIXELS));

    // Prepare macros to access the right light grid/index list.
#ifdef PS_USE_MATERIAL_TRANSPARENCY

#define POINT_LIGHT_GRID             transparentPointLightGrid
#define SPOT_LIGHT_GRID              transparentSpotLightGrid
#define DIRECTIONAL_LIGHT_GRID       transparentDirectionalLightGrid

#define POINT_LIGHT_INDEX_LIST       transparentPointLightIndexList
#define SPOT_LIGHT_INDEX_LIST        transparentSpotLightIndexList
#define DIRECTIONAL_LIGHT_INDEX_LIST transparentDirectionalLightIndexList

#else

#define POINT_LIGHT_GRID             opaquePointLightGrid
#define SPOT_LIGHT_GRID              opaqueSpotLightGrid
#define DIRECTIONAL_LIGHT_GRID       opaqueDirectionalLightGrid

#define POINT_LIGHT_INDEX_LIST       opaquePointLightIndexList
#define SPOT_LIGHT_INDEX_LIST        opaqueSpotLightIndexList
#define DIRECTIONAL_LIGHT_INDEX_LIST opaqueDirectionalLightIndexList

#endif

    // Prepare additional macros to access light grid tile.
#hlsl #define POINT_LIGHT_GRID_TILE        POINT_LIGHT_GRID[tileIndex]
#glsl #define POINT_LIGHT_GRID_TILE        imageLoad(POINT_LIGHT_GRID, tileIndex)
#hlsl #define SPOT_LIGHT_GRID_TILE         SPOT_LIGHT_GRID[tileIndex]
#glsl #define SPOT_LIGHT_GRID_TILE         imageLoad(SPOT_LIGHT_GRID, tileIndex)
#hlsl #define DIRECTIONAL_LIGHT_GRID_TILE  DIRECTIONAL_LIGHT_GRID[tileIndex]
#glsl #define DIRECTIONAL_LIGHT_GRID_TILE  imageLoad(DIRECTIONAL_LIGHT_GRID, tileIndex)

    // Get index into non-culled point lights.
    uint iPointLightIndexListOffset = POINT_LIGHT_GRID_TILE.x;
    uint iPointLightIndexListCount = POINT_LIGHT_GRID_TILE.y;

    // Calculate light from point lights.
    for (uint i = 0; i < iPointLightIndexListCount; i++){
        // Get light index.
        uint iLightIndex = POINT_LIGHT_INDEX_LIST
#glsl                          .array
                               [iPointLightIndexListOffset + i];

        // Calculate light.
        outputColor += calculateColorFromPointLight(
#glsl       pointLights.array[iLightIndex],
#hlsl       pointLights[iLightIndex],
            cameraPosition,
            fragmentPositionWorldSpace,
            fragmentNormalUnit,
            fragmentDiffuseColor,
            fragmentSpecularColor,
            materialRoughness);
    }

    // Get index into non-culled spotlights.
    uint iSpotLightIndexListOffset = SPOT_LIGHT_GRID_TILE.x;
    uint iSpotLightIndexListCount = SPOT_LIGHT_GRID_TILE.y;

    // Calculate light from spotlights.
    for (uint i = 0; i < iSpotLightIndexListCount; i++){
        // Get light index.
        uint iLightIndex = SPOT_LIGHT_INDEX_LIST
#glsl                          .array
                               [iSpotLightIndexListOffset + i];

        // Calculate light.
        outputColor += calculateColorFromSpotlight(
#glsl       spotlights.array[iLightIndex],
#hlsl       spotlights[iLightIndex],
            cameraPosition,
            fragmentPositionWorldSpace,
            fragmentNormalUnit,
            fragmentDiffuseColor,
            fragmentSpecularColor,
            materialRoughness);
    }

    // Get index into non-culled directional lights.
    uint iDirectionalLightIndexListOffset = DIRECTIONAL_LIGHT_GRID_TILE.x;
    uint iDirectionalLightIndexListCount = DIRECTIONAL_LIGHT_GRID_TILE.y;

    // Calculate light from directional lights.
    for (uint i = 0; i < iDirectionalLightIndexListCount; i++){
        // Get light index.
        uint iLightIndex = DIRECTIONAL_LIGHT_INDEX_LIST
#glsl                          .array
                               [iDirectionalLightIndexListOffset + i];

        // Calculate light.
        outputColor += calculateColorFromDirectionalLight(
#glsl       directionalLights.array[iLightIndex],
#hlsl       directionalLights[iLightIndex],
            cameraPosition,
            fragmentPositionWorldSpace,
            fragmentNormalUnit,
            fragmentDiffuseColor,
            fragmentSpecularColor,
            materialRoughness);
    }

    // Apply ambient light.
    outputColor += generalLightingData.ambientLight.rgb * fragmentDiffuseColor;

    return outputColor;
}

#endif