#include "Base.glsl"

#ifdef USE_DIFFUSE_TEXTURE
    layout(binding = 1) uniform sampler2D textureSampler;
#endif

layout(location = 0) in vec4 fragmentViewPosition;
layout(location = 1) in vec4 fragmentWorldPosition;
layout(location = 2) in vec3 fragmentNormal;
layout(location = 3) in vec2 fragmentUv;

layout(location = 0) out vec4 outColor;

/** Describes Material's constants. */
layout(binding = 1) uniform MaterialData
{
    /** Fill color. */
    vec3 diffuseColor;

    /** Opacity (when material transparency is used). */
    float opacity;
} materialData;

void main() {
    outColor = vec4(materialData.diffuseColor, 1.0F);

#ifdef USE_DIFFUSE_TEXTURE
    outColor *= texture(textureSampler, fragmentUv);
#endif
}