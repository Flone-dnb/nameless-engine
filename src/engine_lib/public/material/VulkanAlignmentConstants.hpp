#pragma once

// Standard.
#include <cstddef>

namespace ne {
    /** Scalars have to be aligned by N (= 4 bytes given 32 bit floats). */
    static constexpr size_t iVkScalarAlignment = 4; // NOLINT

    /** A vec2 must be aligned by 2N (= 8 bytes). */
    static constexpr size_t iVkVec2Alignment = 8; // NOLINT

    /** A vec3 or vec4 must be aligned by 4N (= 16 bytes). */
    static constexpr size_t iVkVec4Alignment = 16; // NOLINT

    /** A mat4 matrix must have the same alignment as a vec4. */
    static constexpr size_t iVkMat4Alignment = iVkVec4Alignment;
} // namespace ne
