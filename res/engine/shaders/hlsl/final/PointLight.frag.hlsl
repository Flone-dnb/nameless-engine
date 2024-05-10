#include "../../include/Base.glsl"
#include "../../include/data/ShadowPassLightInfoData.glsl"
#include "../../include/constants/MeshNodeConstants.glsl" // include to make root constants layout as in vertex
#include "../../include/constants/ShadowPassConstants.glsl"

[earlydepthstencil]
/**
 * This function is not expected be called from other shader files.
 *
 * @param pin Fragment.
 *
 * @return Distance from light source to fragment.
 */
float main(VertexOut pin) : SV_Target {
    return length(pin.worldPosition.xyz - shadowPassLightInfo[constants.iShadowPassLightInfoIndex].position.xyz);
}