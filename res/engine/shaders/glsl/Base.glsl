#version 450
#extension GL_ARB_separate_shader_objects : enable

/** Stores frame-global constants. */
layout(binding = 0) uniform FrameData {
    /** Camera's view matrix multiplied by camera's projection matrix. */
    mat4 viewProjectionMatrix;

    /** Camera's world location. */
    vec3 cameraPosition;

    /** Time that has passed since the last frame in seconds (i.e. delta time). */
    float timeSincePrevFrameInSec;

    /** Time since the first window was created (in seconds). */
    float totalTimeInSec;

    vec3 _framePad;
} frameData;