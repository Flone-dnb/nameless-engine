#pragma once

// Standard.
#include <cmath>

// Custom.
#include "math/Vector.hpp"

// External.
#if defined(WIN32)
#include "DirectXMath.h"
#else
#include "math/GLMath.hpp"
#endif

namespace ne {
    /** Static helper functions for math. */
    class MathHelpers {
        MathHelpers() = delete;

        /**
         * Converts radians to degrees.
         *
         * @param radians Radians to convert.
         *
         * @return Degrees.
         */
        static inline float convertRadiansToDegrees(float radians);

        /**
         * Converts degrees to radians.
         *
         * @param degrees Degrees to convert.
         *
         * @return Radians.
         */
        static inline float convertDegreesToRadians(float degrees);

        /**
         * Converts coordinates from the spherical coordinate system to the Cartesian coordinate system.
         *
         * @param radius Radial distance.
         * @param theta  Azimuthal angle.
         * @param phi    Polar angle.
         *
         * @return Coordinates in Cartesian coordinate system.
         */
        static inline Vector convertSphericalToCartesianCoordinates(float radius, float theta, float phi);
    };

    Vector MathHelpers::convertSphericalToCartesianCoordinates(float radius, float theta, float phi) {
        Vector output;
        output.setX(radius * sinf(phi) * cosf(theta));
        output.setY(radius * sinf(phi) * sinf(theta));
        output.setZ(radius * cosf(phi));

        return output;
    }

#if defined(WIN32)
    // --------------------------------------------------------------------------------------------
    // DirectXMath implementation.
    // --------------------------------------------------------------------------------------------

    float MathHelpers::convertRadiansToDegrees(float radians) { return DirectX::XMConvertToDegrees(radians); }

    float MathHelpers::convertDegreesToRadians(float degrees) { return DirectX::XMConvertToRadians(degrees); }
#else
    // --------------------------------------------------------------------------------------------
    // GLM implementation.
    // --------------------------------------------------------------------------------------------

    float MathHelpers::convertRadiansToDegrees(float radians) { return glm::degrees(radians); }

    float MathHelpers::convertDegreesToRadians(float degrees) { return glm::radians(degrees); }
#endif
} // namespace ne
