#ifdef INCLUDE_LIGHTING_FUNCTIONS // define this macro before including to have pixel/fragment light calculation functions
    #ifdef FS_USE_MATERIAL_TRANSPARENCY
        #include "light_culling/TransparentLightGrid.glsl"
    #else
        #include "light_culling/OpaqueLightGrid.glsl"
    #endif
#endif

/** Sampler for shadow maps. */
#hlsl SamplerComparisonState shadowSampler : register(s0, space6);

/** Bias that we use in point light shadow mapping. */
#define POINT_LIGHT_SHADOW_BIAS         0.03F
#define POINT_LIGHT_SHADOW_SLOPE_FACTOR 2.75F

/** Stores general lighting related data. */
#glsl layout(binding = 50) uniform #hlsl struct #both GeneralLightingData {
    /** Light color intensity of ambient lighting. 4th component is not used. */
    vec4 ambientLight;

    /** Total number of spawned point lights in camera frustum. */
    uint iPointLightCountsInCameraFrustum;

    /** Total number of spawned directional lights. */
    uint iDirectionalLightCount;

    /** Total number of spawned spotlights in camera frustum. */
    uint iSpotlightCountsInCameraFrustum;
} #glsl generalLightingData; #hlsl ; ConstantBuffer<GeneralLightingData> generalLightingData : register(b0, space7);

/** Point light parameters. */
struct PointLight {
    /** Light position in world space. 4th component is not used. */
    vec4 position;

    /** Color of the light source. 4th component is not used. */
    vec4 color;

    /** Light intensity, valid values range is [0.0F; 1.0F]. */
    float intensity;

    /** Lit distance. */
    float distance;

    /** Index in the point cube shadow map array where shadow map of this light source is stored. */
    uint iShadowMapIndex;

    #hlsl float pad1; // padding to match C++ struct padding, only in HLSL because `StructuredBuffer` is tightly-packed
};

/** Stores info about all spawned point lights. */
#glsl {
    layout(std140, binding = 51) readonly buffer PointLightsBuffer {
        /** Info about a light. */
        PointLight array[];
    } pointLights;
}
#hlsl StructuredBuffer<PointLight> pointLights : register(t0, space7);

/** Indices to spawned point lights that are in camera's frustum. */
#glsl {
    layout(std430, binding = 52) readonly buffer PointLightsInCameraFrustumBuffer {
        /** Indices into "spawned point lights" array. */
        uint array[];
    } pointLightsInCameraFrustumIndices;
}
#hlsl StructuredBuffer<uint> pointLightsInCameraFrustumIndices : register(t1, space7);

/** Directional light parameters. */
struct DirectionalLight {
    /** Matrix that transforms data (such as positions) to clip (projection) space of the light source. */
    mat4 viewProjectionMatrix;

    /** Light forward unit vector (direction). 4th component is not used. */
    vec4 direction;

    /** Color of the light source. 4th component is not used. */
    vec4 color;

    /** Light intensity, valid values range is [0.0F; 1.0F]. */
    float intensity;

    /** Index in the directional shadow map array where shadow map of this light source is stored. */
    uint iShadowMapIndex;

    #hlsl float2 pad1; // padding to match C++ struct padding, only in HLSL because `StructuredBuffer` is tightly-packed
};

/** Stores info about all spawned directional lights. */
#glsl {
    layout(std140, binding = 53) readonly buffer DirectionalLightsBuffer {
        /** Info about a light. */
        DirectionalLight array[];
    } directionalLights;
}
#hlsl StructuredBuffer<DirectionalLight> directionalLights : register(t2, space7);

/** Spotlight parameters. */
struct Spotlight {
    /** Matrix that transforms data (such as positions) to clip (projection) space of the light source. */
    mat4 viewProjectionMatrix;

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

    /** Index in the spot shadow map array where shadow map of this light source is stored. */
    uint iShadowMapIndex;

    #hlsl float2 pad1; // padding to match C++ struct padding, only in HLSL because `StructuredBuffer` is tightly-packed
};

/** Stores info about all spawned spotlights. */
#glsl {
    layout(std140, binding = 54) readonly buffer SpotlightsBuffer {
        /** Info about a light. */
        Spotlight array[];
    } spotlights;
}
#hlsl StructuredBuffer<Spotlight> spotlights : register(t3, space7);

/** Indices to spawned spotlights that are in camera's frustum. */
#glsl {
    layout(std430, binding = 55) readonly buffer SpotlightsInCameraFrustumBuffer {
        /** Indices into "spawned spotlights" array. */
        uint array[];
    } spotlightsInCameraFrustumIndices;
}
#hlsl StructuredBuffer<uint> spotlightsInCameraFrustumIndices : register(t4, space7);

/** Array of shadow maps for all directional light sources. */
#glsl layout(binding = 57) uniform sampler2DShadow directionalShadowMaps[];
#hlsl Texture2D directionalShadowMaps[] : register(t6, space7);

/** Array of shadow maps for all spotlight sources. */
#glsl layout(binding = 58) uniform sampler2DShadow spotShadowMaps[];
#hlsl Texture2D spotShadowMaps[] : register(t0, space8);

/** Array of shadow maps for all point light sources. */
#glsl layout(binding = 59) uniform samplerCubeShadow pointShadowMaps[];
#hlsl TextureCube pointShadowMaps[] : register(t0, space9);

#ifdef INCLUDE_LIGHTING_FUNCTIONS

    /**
     * Transforms position from world space to shadow map space.
     *
     * @param worldPosition Position in world space to transform.
     * @param viewProjectionMatrix Matrix that transforms positions from world space to projection space.
     *
     * @return Position in shadow map (texture) space.
     */
    vec3 transformWorldPositionToShadowMapSpace(vec3 worldPosition, mat4 viewProjectionMatrix) {
        // Transform to light's projection space.
        vec4 posShadowMapSpace = mul(viewProjectionMatrix, vec4(worldPosition, 1.0F));

        // Perspective divide.
        posShadowMapSpace = posShadowMapSpace / posShadowMapSpace.w;

        // Transform to texture space (shadow map space).
        posShadowMapSpace.x = (posShadowMapSpace.x + 1.0F) / 2.0F;  // converts from [-1..1] to [0..1]
        posShadowMapSpace.y = (posShadowMapSpace.y - 1.0F) / -2.0F; // converts from [-1..1] to [0..1] and "flips" Y
        // because of Y axis difference in NDC and texture space

        return posShadowMapSpace.xyz;
    }

    /**
     * Defines a `shadowFactor` value in range [0.0; 1.0] where 0.0 means that the fragment is fully in the shadow
     * created by the light source and 1.0 means that the fragment is not in the shadow created by the light source.
     * 
     * @param fragmentWorldPosition Position of the fragment in world space.
     * @param iShadowMapIndex       Shodow map index in the array.
     * @param viewProjectionMatrix  Matrix that transforms positions to light's projection space.
     * @param shadowMapArray        Array with shadow maps to use.
     */
    #define DEFINE_SHADOW_FACTOR_DIRECTIONAL_SPOT_LIGHT(fragmentWorldPosition, iShadowMapIndex, viewProjectionMatrix, shadowMapArray) \
    /** Get shadow map size. */                                                                                          \
    uint iShadowMapWidth = 0;                                                                                            \
    uint iShadowMapHeight = 0;                                                                                           \
    uint iShadowMapMipCount = 0;                                                                                         \
    #hlsl shadowMapArray[iShadowMapIndex].GetDimensions(0, iShadowMapWidth, iShadowMapHeight, iShadowMapMipCount);       \
    #glsl iShadowMapWidth = textureSize(shadowMapArray[iShadowMapIndex], 0).x;                                           \
    /** Transform fragment to shadow map space. */                                                                       \
    const vec3 fragPosShadowMapSpace                                                                                     \
        = transformWorldPositionToShadowMapSpace(fragmentWorldPosition, viewProjectionMatrix);                           \
    /** Calcualte texel size. */                                                                                         \
    const float texelSize = 1.0F /                                                                                       \
    #hlsl     (float)iShadowMapWidth;                                                                                    \
    #glsl     float(iShadowMapWidth);                                                                                    \
    /** Prepare positions of the shadow map to sample using PCF per each position. */                                    \
    /** Sample multiple near/close positions for better shadow anti-aliasing */                                          \
    /** (too much sample points might cause shadow acne). */                                                             \
    const uint iShadowFilteringOffsetCount = 9; /** 3x3 kernel */                                                        \
    const vec2 vShadowFilteringOffsets[iShadowFilteringOffsetCount] = {\
        vec2(-texelSize, -texelSize), vec2(0.0F, -texelSize), vec2(texelSize, -texelSize),                               \
        vec2(-texelSize, 0.0F),       vec2(0.0F, 0.0F),       vec2(texelSize, 0.0F),                                     \
        vec2(-texelSize, texelSize),  vec2(0.0F, texelSize),  vec2(texelSize, texelSize)                                 \
    };                                                                                                                   \
    /** Compare fragment depth with shadow map depth. */                                                                 \
    float shadowPercentLit = 0.0F;                                                                                       \
    for(uint i = 0; i < iShadowFilteringOffsetCount; i++) {\
        /** Prepare sample point. */                                                                                     \
        const vec2 samplePoint = fragPosShadowMapSpace.xy + vShadowFilteringOffsets[i];                                  \
        /** Do 4-point PCF. */                                                                                           \
        shadowPercentLit +=                                                                                              \
        #hlsl shadowMapArray[iShadowMapIndex].SampleCmpLevelZero(shadowSampler, samplePoint, fragPosShadowMapSpace.z);   \
        #glsl texture(shadowMapArray[iShadowMapIndex], vec3(samplePoint, fragPosShadowMapSpace.z));                      \
    }                                                                                                                    \
    /** Normalize the result. */                                                                                         \
    const float shadowFactor = shadowPercentLit /                                                                        \
    #hlsl (float)iShadowFilteringOffsetCount;
    #glsl float(iShadowFilteringOffsetCount);

    /**
     * Calculates a value in range [0.0; 1.0] where 0.0 means that the fragment is fully in the shadow
     * created by the light source and 1.0 means that the fragment is not in the shadow created by the light source.
     * 
     * @param fragmentWorldPosition Position of the fragment in world space.
     * @param fragmentNormalUnit    Normalized normal vector of the fragment in world space.
     * @param light                 Point light to calculate shadow factor for.
     *
     * @return Shadow factor.
     */
    float calculateShadowFactorForPointLight(vec3 fragmentWorldPosition, vec3 fragmentNormalUnit, PointLight light) {
        // Get shadow map size.
        uint iShadowMapWidth = 0;
        uint iShadowMapHeight = 0;
        uint iShadowMapMipCount = 0;
        #hlsl pointShadowMaps[light.iShadowMapIndex].GetDimensions(0, iShadowMapWidth, iShadowMapHeight, iShadowMapMipCount);
        #glsl iShadowMapWidth = textureSize(pointShadowMaps[light.iShadowMapIndex], 0).x;

        // Calcualte texel size.
        const float texelSize = 1.0F /
        #hlsl     (float)iShadowMapWidth;
        #glsl     float(iShadowMapWidth);

        // Calculate direction from light to fragment.
        const vec3 lightToFragmentDirection = fragmentWorldPosition - light.position.xyz;

        // Calculate additional point light specific slope based bias factor.
        const float biasSlopeFactor = 1.0F - abs(dot(normalize(lightToFragmentDirection), fragmentNormalUnit));

        // Calculate bias that changes according to the lit distance.
        const float baseBias = POINT_LIGHT_SHADOW_BIAS * (light.distance / 11.0F); // magic number

        // Calculate the final bias.
        const float finalBias
            = baseBias + (baseBias * POINT_LIGHT_SHADOW_SLOPE_FACTOR * biasSlopeFactor) + baseBias * 1500.0F * texelSize;

        // Calculate distance between light and fragment.
        const float distanceToLight = length(lightToFragmentDirection) - finalBias;

        // Sample length (distance) from cubemap and compare with our distance.
        const float shadowPercentLit =
        #hlsl pointShadowMaps[light.iShadowMapIndex].SampleCmpLevelZero(shadowSampler, lightToFragmentDirection, distanceToLight);
        #glsl texture(pointShadowMaps[light.iShadowMapIndex], vec4(lightToFragmentDirection, distanceToLight));

        return shadowPercentLit;
    }

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
    vec3 calculateSpecularLight(vec3 fragmentSpecularColor, vec3 fragmentHalfwayVector, vec3 lightDirectionUnit) {
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
        float materialRoughness) {
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
            = (smoothness + 8.0F) * pow(max(dot(fragmentLightHalfwayDirectionUnit, fragmentNormalUnit), 0.0F), smoothness) / 8.0F;

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
    float calculateLightAttenuation(float distanceToLightSource, float lightIntensity, float lightDistance) {
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
        float materialRoughness) {
        // Calculate light attenuation.
        float fragmentDistanceToLight = length(lightSource.position.xyz - fragmentPosition);
        vec3 attenuatedLightColor
            = lightSource.color.rgb * calculateLightAttenuation(fragmentDistanceToLight, lightSource.intensity, lightSource.distance);

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
        float materialRoughness) {
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
        float materialRoughness) {
        // Calculate light attenuation.
        float fragmentDistanceToLight = length(lightSource.position.xyz - fragmentPosition);
        vec3 attenuatedLightColor
            = lightSource.color.rgb * calculateLightAttenuation(fragmentDistanceToLight, lightSource.intensity, lightSource.distance);

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
        float materialRoughness) {
        // Prepare final color.
        vec3 outputColor = vec3(0.0F, 0.0F, 0.0F);

        // Calculate light grid tile index.
        #hlsl uint2 tileIndex = uint2 #glsl ivec2 tileIndex = ivec2 #both (
            floor(fragmentPositionScreenSpace / LIGHT_GRID_TILE_SIZE_IN_PIXELS));

        // Prepare macros to access the right light grid/index list.
        #ifdef FS_USE_MATERIAL_TRANSPARENCY
            #define POINT_LIGHT_GRID         transparentPointLightGrid
            #define SPOT_LIGHT_GRID          transparentSpotLightGrid

            #define POINT_LIGHT_INDEX_LIST   transparentPointLightIndexList
            #define SPOT_LIGHT_INDEX_LIST    transparentSpotLightIndexList
        #else // opaque material
            #define POINT_LIGHT_GRID         opaquePointLightGrid
            #define SPOT_LIGHT_GRID          opaqueSpotLightGrid

            #define POINT_LIGHT_INDEX_LIST   opaquePointLightIndexList
            #define SPOT_LIGHT_INDEX_LIST    opaqueSpotLightIndexList
        #endif

        // Prepare additional macros to access light grid tile.
        #hlsl #define POINT_LIGHT_GRID_TILE POINT_LIGHT_GRID[tileIndex]
        #glsl #define POINT_LIGHT_GRID_TILE imageLoad(POINT_LIGHT_GRID, tileIndex)
        #hlsl #define SPOT_LIGHT_GRID_TILE  SPOT_LIGHT_GRID[tileIndex]
        #glsl #define SPOT_LIGHT_GRID_TILE  imageLoad(SPOT_LIGHT_GRID, tileIndex)

        // Get index into non-culled point lights.
        uint iPointLightIndexListOffset = POINT_LIGHT_GRID_TILE.x;
        uint iPointLightIndexListCount = POINT_LIGHT_GRID_TILE.y;

        // Prepare helper macro.
        #glsl #define POINT_LIGHTS_ARRAY pointLights.array
        #hlsl #define POINT_LIGHTS_ARRAY pointLights

        // Calculate light from point lights.
        for (uint i = 0; i < iPointLightIndexListCount; i++) {
            // Get light index.
            uint iLightIndex = POINT_LIGHT_INDEX_LIST
            #glsl .array
            [iPointLightIndexListOffset + i];

            // Get light source.
            PointLight lightSource = POINT_LIGHTS_ARRAY[iLightIndex];

            // Calculate shadow from this light.
            const float shadowFactor = calculateShadowFactorForPointLight(fragmentPositionWorldSpace, fragmentNormalUnit, lightSource);

            // Calculate light.
            const vec3 light = calculateColorFromPointLight(
                lightSource,
                cameraPosition,
                fragmentPositionWorldSpace,
                fragmentNormalUnit,
                fragmentDiffuseColor,
                fragmentSpecularColor,
                materialRoughness);

            // Combine light with shadow.
            outputColor += light * shadowFactor;
        }

        // Get index into non-culled spotlights.
        uint iSpotLightIndexListOffset = SPOT_LIGHT_GRID_TILE.x;
        uint iSpotLightIndexListCount = SPOT_LIGHT_GRID_TILE.y;

        // Prepare helper macro.
        #glsl #define SPOT_LIGHTS_ARRAY spotlights.array
        #hlsl #define SPOT_LIGHTS_ARRAY spotlights

        // Calculate light from spotlights.
        for (uint i = 0; i < iSpotLightIndexListCount; i++) {
            // Get light index.
            uint iLightIndex = SPOT_LIGHT_INDEX_LIST
            #glsl .array
            [iSpotLightIndexListOffset + i];

            // Get light source.
            Spotlight lightSource = SPOT_LIGHTS_ARRAY[iLightIndex];

            // Calculate shadow from this light.
            DEFINE_SHADOW_FACTOR_DIRECTIONAL_SPOT_LIGHT(
                fragmentPositionWorldSpace,
                lightSource.iShadowMapIndex,
                lightSource.viewProjectionMatrix,
                spotShadowMaps);

            // Calculate light.
            const vec3 light = calculateColorFromSpotlight(
                lightSource,
                cameraPosition,
                fragmentPositionWorldSpace,
                fragmentNormalUnit,
                fragmentDiffuseColor,
                fragmentSpecularColor,
                materialRoughness);

            // Combine light with shadow.
            outputColor += light * shadowFactor;
        }

        // Prepare helper macro.
        #glsl #define DIRECTIONAL_LIGHTS_ARRAY directionalLights.array
        #hlsl #define DIRECTIONAL_LIGHTS_ARRAY directionalLights

        // Calculate light from directional lights.
        for (uint iLightIndex = 0; iLightIndex < generalLightingData.iDirectionalLightCount; iLightIndex++) {
            // Get light source.
            DirectionalLight lightSource = DIRECTIONAL_LIGHTS_ARRAY[iLightIndex];

            // Calculate shadow from this light.
            DEFINE_SHADOW_FACTOR_DIRECTIONAL_SPOT_LIGHT(
                fragmentPositionWorldSpace,
                lightSource.iShadowMapIndex,
                lightSource.viewProjectionMatrix,
                directionalShadowMaps);

            // Calculate light from this light.
            const vec3 light = calculateColorFromDirectionalLight(
                lightSource,
                cameraPosition,
                fragmentPositionWorldSpace,
                fragmentNormalUnit,
                fragmentDiffuseColor,
                fragmentSpecularColor,
                materialRoughness);

            // Combine light with shadow.
            outputColor += light * shadowFactor;
        }

        // Apply ambient light.
        outputColor += generalLightingData.ambientLight.rgb * fragmentDiffuseColor;

        return outputColor;
    }

#endif