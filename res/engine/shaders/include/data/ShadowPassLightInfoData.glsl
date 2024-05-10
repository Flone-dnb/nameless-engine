#if defined(VS_SHADOW_MAPPING_PASS) || defined(POINT_LIGHT_SHADOW_PASS)
    
    /** Groups information used for lights in shadow pass. */
    struct ShadowPassLightInfo {
        /** Matrix that transforms positions from world space to light's projection space. */
        mat4 viewProjectionMatrix;
        
        /** World location of the light source. 4th component is not used. */
        vec4 position;
    };
    
    /** Bindless binded info for all light sources. */
    #glsl {
        layout(std140, binding = 56) readonly buffer ShadowPassLightInfoBuffer {
            /** Shadow pass related info for a light source. */
            ShadowPassLightInfo array[];
        } shadowPassLightInfo;
    }
    #hlsl StructuredBuffer<ShadowPassLightInfo> shadowPassLightInfo : register(t5, space7);
    
#endif