#pragma once

// Standard.
#include <cmath>

// External.
#include "math/GLMath.hpp"

namespace ne {
    /** Static helper functions for math. */
    class MathHelpers {
    public:
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

        /**
         * Converts coordinates from the Cartesian coordinate system to spherical coordinate system.
         *
         * @param location Location in Cartesian coordinate system.
         * @param radius   Calculated radius in spherical coordinate system.
         * @param theta    Calculated theta in spherical coordinate system.
         * @param phi      Calculated phi in spherical coordinate system.
         */
        static inline void convertCartesianCoordinatesToSpherical(
            const glm::vec3& location, float& radius, float& theta, float& phi);
    };

    glm::vec3 MathHelpers::convertSphericalToCartesianCoordinates(float radius, float theta, float phi) {
        const auto sinPhi = std::sinf(phi);
        const auto sinTheta = std::sinf(theta);
        const auto cosPhi = std::cosf(phi);
        const auto cosTheta = std::cosf(theta);
        return glm::vec3(radius * sinPhi * cosTheta, radius * sinPhi * sinTheta, radius * cosPhi);
    }

    void MathHelpers::convertCartesianCoordinatesToSpherical(
        const glm::vec3& location, float& radius, float& theta, float& phi) {
        radius = glm::sqrt(location.x * location.x + location.y * location.y + location.z * location.z);
        theta = glm::atan2(location.y, location.x);
        phi = glm::atan2(glm::sqrt(location.x * location.x + location.y * location.y), location.z);
    }
} // namespace ne
