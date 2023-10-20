/**
 * GLM headers that the engine uses with needed predefined macros.
 * Prefer to include this header instead of the original GLM headers.
 */

#pragma once

#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_INLINE
#define GLM_FORCE_XYZW_ONLY
#define GLM_ENABLE_EXPERIMENTAL
// #define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES  // Disabling these because they have proved to cause
// #define GLM_FORCE_INTRINSICS                // some absurd crashed only in release builds which are
// probably caused by me not following some rules for aligned types but in order to make the life easier I'm
// disabling them. In addition, we also want to avoid using these macros to avoid bloating the data
// we are passing to shaders and since we specify `alignas` on all fields of types that will be copied
// to the GPU we expect no auto-padding and have a fine control of the alignment.

#include "glm/glm.hpp"
#include "glm/trigonometric.hpp"
#include "glm/gtx/vector_angle.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/compatibility.hpp"
