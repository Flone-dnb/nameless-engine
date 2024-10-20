/**
 * Converts the specified coordinates from NDC space to view space.
 *
 * @param ndcSpaceCoordinates     Coordinates in screen space with depth in NDC space (since there's really no depth in screen space).
 * @param inverseProjectionMatrix Inverse of the projection matrix.
 *
 * @return Coordinates in view space.
 */
vec4 convertNdcSpaceToViewSpace(vec3 ndcSpaceCoordinates, mat4 inverseProjectionMatrix) {
    // Undo projection
    // (NDC `w` is always 1 because after perspective divide).
    vec4 viewSpaceCoordinates = mul(inverseProjectionMatrix, vec4(ndcSpaceCoordinates, 1.0F));

    // Undo perspective divide.
    viewSpaceCoordinates = viewSpaceCoordinates / viewSpaceCoordinates.w;

    return viewSpaceCoordinates;
}

/**
 * Converts the specified coordinates from screen space to view space.
 *
 * @param screenSpaceCoordinates  Coordinates in screen space with depth in NDC space (since there's really no depth in screen space).
 * @param iRenderResolutionWidth  Width of the viewport (might be smaller that the actual screen size).
 * @param iRenderResolutionHeight Height of the viewport (might be smaller that the actual screen size).
 * @param inverseProjectionMatrix Inverse of the projection matrix.
 *
 * @return Coordinates in view space.
 */
vec4 convertScreenSpaceToViewSpace(
    vec3 screenSpaceCoordinates,
    uint iRenderResolutionWidth,
    uint iRenderResolutionHeight,
    mat4 inverseProjectionMatrix) {
    // Convert screen coordinates to normalized coordinates in range [0..1; 0..1].
    vec2 screenNormalized = vec2(screenSpaceCoordinates.x / iRenderResolutionWidth, screenSpaceCoordinates.y / iRenderResolutionHeight);

    // Calculate coordinates in NDC space.
    vec3 ndcSpaceCoordinates = vec3(
        // converts from [0..1] to [-1..1]
        screenNormalized.x * 2.0F - 1.0F,
        // "flips" (not an actual flip) Y since in screen space it goes down but
        // in NDC it goes up and converts from [0..1] to [-1..1]
        -2.0F * screenNormalized.y + 1.0F,
        // copy z
        screenSpaceCoordinates.z);

    return convertNdcSpaceToViewSpace(ndcSpaceCoordinates, inverseProjectionMatrix);
}