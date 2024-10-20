/** Global counters into the light index lists. */
#glsl layout(std140, binding = 6) buffer #hlsl struct #both GlobalCountersIntoLightIndexList {
    uint iPointLightListOpaque;
    uint iSpotlightListOpaque;

    uint iPointLightListTransparent;
    uint iSpotlightListTransparent;
} #glsl globalCountersIntoLightIndexList; #hlsl ; RWStructuredBuffer<GlobalCountersIntoLightIndexList> globalCountersIntoLightIndexList : register(u0, space6);

/** Resets (sets to zero) all global counters into light index lists. */
void resetGlobalLightIndexListCounters() {
    // Prepare a helper macro.
    #hlsl #define COUNTERS globalCountersIntoLightIndexList[0]
    #glsl #define COUNTERS globalCountersIntoLightIndexList

    COUNTERS.iPointLightListOpaque = 0;
    COUNTERS.iSpotlightListOpaque = 0;

    COUNTERS.iPointLightListTransparent = 0;
    COUNTERS.iSpotlightListTransparent = 0;
}