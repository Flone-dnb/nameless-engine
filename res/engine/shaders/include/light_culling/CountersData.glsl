/** Global counters into the light index lists. */
#hlsl struct GlobalCountersIntoLightIndexList{
#glsl layout(std140, binding = 6) buffer GlobalCountersIntoLightIndexList{
    uint iPointLightListOpaque;
    uint iSpotlightListOpaque;
    uint iDirectionalLightListOpaque;

    uint iPointLightListTransparent;
    uint iSpotlightListTransparent;
    uint iDirectionalLightListTransparent;
#hlsl }; RWStructuredBuffer<GlobalCountersIntoLightIndexList> globalCountersIntoLightIndexList : register(u0, space6);
#glsl } globalCountersIntoLightIndexList;

/** Resets (sets to zero) all global counters into light index lists. */
void resetGlobalLightIndexListCounters(){
    // Prepare a helper macro.
#hlsl #define COUNTERS globalCountersIntoLightIndexList[0]
#glsl #define COUNTERS globalCountersIntoLightIndexList

    COUNTERS.iPointLightListOpaque = 0;
    COUNTERS.iSpotlightListOpaque = 0;
    COUNTERS.iDirectionalLightListOpaque = 0;

    COUNTERS.iPointLightListTransparent = 0;
    COUNTERS.iSpotlightListTransparent = 0;
    COUNTERS.iDirectionalLightListTransparent = 0;
}