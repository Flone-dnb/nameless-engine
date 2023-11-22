// Include this file to get frame constants.

/** Stores frame-global constants. */
#glsl layout(binding = 0) uniform FrameData {
#hlsl struct FrameData{
    /** Camera's view matrix. */
    mat4 viewMatrix;
    
    /** Camera's view matrix multiplied by camera's projection matrix. */
    mat4 viewProjectionMatrix;

    /** Camera's world location. 4th component is not used. */
    vec4 cameraPosition;

    /** Time that has passed since the last frame in seconds (i.e. delta time). */
    float timeSincePrevFrameInSec;

    /** Time since the first window was created (in seconds). */
    float totalTimeInSec;
#glsl } frameData;
#hlsl }; ConstantBuffer<FrameData> frameData : register(b0, space5);