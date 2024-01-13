#include "../../include/Base.glsl"
#include "../../include/ShadowPassLightInfoData.glsl"
#include "../../include/shader_constants/MeshNodeShaderConstants.glsl" // include to make push constants layout as in vertex
#include "../../include/shader_constants/ShadowPassConstants.glsl"

/** Input parameters. */
layout(location = 0) in vec4 fragmentWorldPosition;

/** Output parameters. */
layout(location = 0) out float outputValue;

/** Fragment shader. */
layout(early_fragment_tests) in;
void main(){
    // Store distance from light to fragment as a resulting value.
    outputValue = length(fragmentWorldPosition.xyz - shadowPassLightInfo.array[constants.iShadowPassLightInfoIndex].position.xyz);
}