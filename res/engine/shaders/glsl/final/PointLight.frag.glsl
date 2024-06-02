#include "../../include/Base.glsl"
#include "../../include/data/ShadowPassLightInfoData.glsl"
#include "../../include/constants/MeshNodeConstants.glsl" // include to make push constants layout as in vertex
#include "../../include/constants/ShadowPassConstants.glsl"

#define LAYOUT_FRAGMENT_SHADER
#include "../formats/MeshNodeVertexLayout.glsl"

layout(location = 0) out float outputValue;

layout(early_fragment_tests) in;
/**
 * This fragment shader is used during shadow mapping: for point lights we draw all meshes using their usual vertex shader and a special
 * fragment shader (this one).
 *
 * @remark This function is not expected be called from other shader files.
 */
void main() {
    // Store distance from light to fragment as a resulting value.
    outputValue = length(
        fragmentWorldPosition.xyz - shadowPassLightInfo.array[constants.iShadowPassLightInfoIndex].position.xyz);
}