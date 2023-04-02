#pragma once

// Standard.
#include <cmath>

// Custom.
#include "misc/Globals.h"
#if defined(DEBUG)
#include "io/Logger.h"
#endif

// External.
#include "math/GLMath.hpp"

namespace ne {
    /** Static helper functions for math. */
    class MathHelpers {
    public:
        MathHelpers() = delete;

        /**
         * Converts a direction to angles.
         *
         * @warning Expects the specified direction to be normalized.
         *
         * @param direction Normalized direction to convert.
         *
         * @return Roll (as X), pitch (as Y) and yaw (as Z) in degrees.
         */
        static inline glm::vec3 convertDirectionToRollPitchYaw(const glm::vec3& direction);

        /**
         * Converts coordinates from the spherical coordinate system to the Cartesian coordinate system.
         *
         * @param radius Radial distance.
         * @param theta  Azimuthal angle (in degrees).
         * @param phi    Polar angle (in degrees).
         *
         * @return Coordinates in Cartesian coordinate system.
         */
        static inline glm::vec3 convertSphericalToCartesianCoordinates(float radius, float theta, float phi);

        /**
         * Converts coordinates from the Cartesian coordinate system to spherical coordinate system.
         *
         * @param location Location in Cartesian coordinate system.
         * @param radius   Calculated radius in spherical coordinate system.
         * @param theta    Calculated theta in spherical coordinate system (in degrees).
         * @param phi      Calculated phi in spherical coordinate system (in degrees).
         */
        static inline void convertCartesianCoordinatesToSpherical(
            const glm::vec3& location, float& radius, float& theta, float& phi);

    private:
#if defined(DEBUG)
        /** Name of the category used for logging. */
        static inline const auto sMathHelpersLogCategory = "Math Helpers";
#endif
    };

    glm::vec3 MathHelpers::convertDirectionToRollPitchYaw(const glm::vec3& direction) {
#if defined(DEBUG)
        // Make sure we are given a normalized vector.
        if (!glm::epsilonEqual(glm::length(direction), 1.0F, 0.001F)) { // NOLINT: magic number
            Logger::get().error(
                "the specified direction vector should have been normalized", sMathHelpersLogCategory);
            return glm::vec3(0.0F, 0.0F, 0.0F);
        }
#endif

        glm::vec3 worldRotation = glm::vec3(0.0F, 0.0F, 0.0F);

        worldRotation.z = glm::degrees(std::atan2f(direction.y, direction.x));
        worldRotation.y = glm::degrees(-std::asinf(direction.z));

#if defined(DEBUG)
        // Check for NaNs.
        if (glm::isnan(worldRotation.z)) {
            Logger::get().warn(
                "found NaN in the Z component of the calculated rotation", sMathHelpersLogCategory);
        }
        if (glm::isnan(worldRotation.y)) {
            Logger::get().warn(
                "found NaN in the Y component of the calculated rotation", sMathHelpersLogCategory);
        }
#endif

        // Use zero roll for now.

        // Calculate roll:
        // See if we can use world up direction to find the right direction.
        // glm::vec3 vecToFindRight = worldUpDirection;
        // if (direction.z > 0.999F) { // NOLINT: magic number
        //    // Use +X then.
        //    vecToFindRight = glm::vec3(1.0F, 0.0F, 0.0F);
        //}
        // const auto rightDirection = glm::normalize(glm::cross(vecToFindRight, direction));

        // worldRotation.x =
        //     glm::degrees(-std::asinf(worldUpDirection.x * rightDirection.x + worldUpDirection.y *
        //     rightDirection.y));

        // Check roll for NaN.
        // if (glm::isnan(worldRotation.x)) {
        //     worldRotation.x = 0.0F;
        // }

        return worldRotation;
    }

    glm::vec3 MathHelpers::convertSphericalToCartesianCoordinates(float radius, float theta, float phi) {
        phi = glm::radians(phi);
        theta = glm::radians(theta);

        const auto sinPhi = std::sinf(phi);
        const auto sinTheta = std::sinf(theta);
        const auto cosPhi = std::cosf(phi);
        const auto cosTheta = std::cosf(theta);
        return glm::vec3(radius * sinPhi * cosTheta, radius * sinPhi * sinTheta, radius * cosPhi);
    }

    void MathHelpers::convertCartesianCoordinatesToSpherical(
        const glm::vec3& location, float& radius, float& theta, float& phi) {
        phi = glm::radians(phi);
        theta = glm::radians(theta);

        radius = glm::sqrt(location.x * location.x + location.y * location.y + location.z * location.z);
        theta = glm::atan2(location.y, location.x);
        phi = glm::atan2(glm::sqrt(location.x * location.x + location.y * location.y), location.z);
    }
} // namespace ne
