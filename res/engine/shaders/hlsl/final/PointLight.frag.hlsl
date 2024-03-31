#include "../../include/Base.glsl"
#include "../../include/data/ShadowPassLightInfoData.glsl"
#include "../../include/constants/MeshNodeConstants.glsl" // include to make root constants layout as in vertex
#include "../../include/constants/ShadowPassConstants.glsl"

[earlydepthstencil]
float main(VertexOut pin) : SV_Target{
    // Store distance from light to fragment as a resulting value.
    return length(pin.worldPosition.xyz - shadowPassLightInfo[constants.iShadowPassLightInfoIndex].position.xyz);
}