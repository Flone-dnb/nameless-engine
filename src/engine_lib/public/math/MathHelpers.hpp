#pragma once

// Standard.
#include <cmath>

// External.
#include "math/GLMath.hpp"

namespace ne {
    /** Static helper functions for math. */
    class MathHelpers {
        MathHelpers() = delete;

        /**
         * Converts coordinates from the spherical coordinate system to the Cartesian coordinate system.
         *
         * @param radius Radial distance.
         * @param theta  Azimuthal angle.
         * @param phi    Polar angle.
         *
         * @return Coordinates in Cartesian coordinate system.
         */
        static inline glm::vec3 convertSphericalToCartesianCoordinates(float radius, float theta, float phi);
    };

    glm::vec3 MathHelpers::convertSphericalToCartesianCoordinates(float radius, float theta, float phi) {
        return glm::vec3(
            radius * sinf(phi) * cosf(theta), radius * sinf(phi) * sinf(theta), radius * cosf(phi));
        ;
    }
} // namespace ne
