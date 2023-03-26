/**
 * GLM headers that the engine uses with needed predefined macros.
 * Prefer to include this header instead of the original GLM headers.
 */

#pragma once

#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_INLINE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES // don't remove this as some structs that we pass to shaders
                                           // rely on this auto-padding (we don't pad structs manually)
#define GLM_FORCE_INTRINSICS
#include "glm/glm.hpp"
#include "glm/trigonometric.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/vector_angle.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtx/compatibility.hpp"
