#pragma once

// Standard.
#include <cmath>

// Custom.
#include "misc/Globals.h"
#include "io/Logger.h"

// External.
#include "math/GLMath.hpp"

namespace ne {
    /** Static helper functions for math. */
    class MathHelpers {
    public:
        MathHelpers() = delete;

        /**
         * Converts a direction to rotation angles.
         *
         * @warning Expects the specified direction to be normalized.
         *
         * @param direction Normalized direction to convert.
         *
         * @return Roll (as X), pitch (as Y) and yaw (as Z) in degrees.
         */
        static inline glm::vec3 convertDirectionToRollPitchYaw(const glm::vec3& direction);

        /**
         * Converts rotation angles to a direction.
         *
         * @param rotation Rotation roll (as X), pitch (as Y) and yaw (as Z) in degrees.
         *
         * @return Unit direction vector.
         */
        static inline glm::vec3 convertRollPitchYawToDirection(const glm::vec3& rotation);

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

        /**
         * Calculates 1 / vector while checking for zero division.
         *
         * @param vector Input vector.
         *
         * @return vector 1 / input vector.
         */
        static inline glm::vec3 calculateReciprocalVector(const glm::vec3& vector);

        /**
         * Builds a rotation matrix in the engine specific way.
         *
         * @param rotation Rotation in degrees where X is roll, Y is pitch and Z is yaw.
         *
         * @return Rotation matrix.
         */
        static inline glm::mat4x4 buildRotationMatrix(const glm::vec3& rotation);

        /**
         * Changes the value to be in the range [min; max].
         *
         * Example:
         * @code
         * normalizeValue(370.0F, -360.0F, 360.0F); // result is `-350`
         *
         * normalizeValue(-730, -360, 360); // result is `-10`
         * @endcode
         *
         * @param value Value to normalize.
         * @param min   Minimum value in range.
         * @param max   Maximum value in range.
         *
         * @return Normalized value.
         */
        static inline float normalizeValue(float value, float min, float max);

        /**
         * Normalizes the specified vector while checking for zero division (to avoid NaNs in
         * the normalized vector).
         *
         * @param vector Input vector to normalize.
         *
         * @return Normalized vector.
         */
        static inline glm::vec3 normalizeSafely(const glm::vec3& vector);

    private:
        /** Default tolerance for floats to use. */
        static inline const float smallFloatEpsilon = 0.0000001F; // NOLINT: not a very small number
    };

    glm::vec3 MathHelpers::convertDirectionToRollPitchYaw(const glm::vec3& direction) {
        if (glm::all(glm::epsilonEqual(direction, glm::vec3(0.0F, 0.0F, 0.0F), smallFloatEpsilon))) {
            return glm::vec3(0.0F, 0.0F, 0.0F);
        }

#if defined(DEBUG)
        // Make sure we are given a normalized vector.
        constexpr float lengthDelta = 0.001F; // NOLINT: don't use too small value here
        const auto length = glm::length(direction);
        if (!glm::epsilonEqual(length, 1.0F, lengthDelta)) [[unlikely]] {
            Logger::get().error("the specified direction vector should have been normalized");
        }
#endif

        glm::vec3 worldRotation = glm::vec3(0.0F, 0.0F, 0.0F);

        worldRotation.z = glm::degrees(std::atan2(direction.y, direction.x));
        worldRotation.y = glm::degrees(-std::asin(direction.z));

        // Check for NaNs.
        if (glm::isnan(worldRotation.z)) {
            Logger::get().warn(
                "found NaN in the Z component of the calculated rotation, setting this component's value to "
                "zero");
            worldRotation.z = 0.0F;
        }
        if (glm::isnan(worldRotation.y)) {
            Logger::get().warn(
                "found NaN in the Y component of the calculated rotation, setting this component's value to "
                "zero");
            worldRotation.y = 0.0F;
        }

        // Use zero roll for now.

        // Calculate roll:
        // See if we can use world up direction to find the right direction.
        // glm::vec3 vecToFindRight = worldUpDirection;
        // if (std::abs(direction.z) > 0.999F) { // NOLINT: magic number
        //    // Use +X then.
        //    vecToFindRight = glm::vec3(1.0F, 0.0F, 0.0F);
        //}
        // const auto rightDirection = glm::normalize(glm::cross(direction, vecToFindRight));

        // worldRotation.x =
        //     glm::degrees(-std::asinf(worldUpDirection.x * rightDirection.x + worldUpDirection.y *
        //     rightDirection.y));

        // Check roll for NaN.
        // if (glm::isnan(worldRotation.x)) {
        //     worldRotation.x = 0.0F;
        // }

        return worldRotation;
    }

    glm::vec3 MathHelpers::convertRollPitchYawToDirection(const glm::vec3& rotation) {
        return buildRotationMatrix(rotation) * glm::vec4(Globals::WorldDirection::forward, 0.0F);
    }

    glm::vec3 MathHelpers::convertSphericalToCartesianCoordinates(float radius, float theta, float phi) {
        phi = glm::radians(phi);
        theta = glm::radians(theta);

        const auto sinPhi = std::sin(phi);
        const auto sinTheta = std::sin(theta);
        const auto cosPhi = std::cos(phi);
        const auto cosTheta = std::cos(theta);
        return glm::vec3(radius * sinPhi * cosTheta, radius * sinPhi * sinTheta, radius * cosPhi);
    }

    void MathHelpers::convertCartesianCoordinatesToSpherical(
        const glm::vec3& location, float& radius, float& theta, float& phi) {
        radius = glm::sqrt(location.x * location.x + location.y * location.y + location.z * location.z);
        theta = glm::degrees(glm::atan2(location.y, location.x));
        phi = glm::degrees(
            glm::atan2(glm::sqrt(location.x * location.x + location.y * location.y), location.z));
    }

    glm::vec3 MathHelpers::calculateReciprocalVector(const glm::vec3& vector) {
        glm::vec3 reciprocal;

        if (std::abs(vector.x) < smallFloatEpsilon) [[unlikely]] {
            reciprocal.x = 0.0F;
        } else [[likely]] {
            reciprocal.x = 1.0F / vector.x;
        }

        if (std::abs(vector.y) < smallFloatEpsilon) [[unlikely]] {
            reciprocal.y = 0.0F;
        } else [[likely]] {
            reciprocal.y = 1.0F / vector.y;
        }

        if (std::abs(vector.z) < smallFloatEpsilon) [[unlikely]] {
            reciprocal.z = 0.0F;
        } else [[likely]] {
            reciprocal.z = 1.0F / vector.z;
        }

        return reciprocal;
    }

    glm::mat4x4 MathHelpers::buildRotationMatrix(const glm::vec3& rotation) {
        return glm::rotate(glm::radians(rotation.z), glm::vec3(0.0F, 0.0F, 1.0F)) *
               glm::rotate(glm::radians(rotation.y), glm::vec3(0.0F, 1.0F, 0.0F)) *
               glm::rotate(glm::radians(rotation.x), glm::vec3(1.0F, 0.0F, 0.0F));
    }

    float MathHelpers::normalizeValue(float value, float min, float max) {
        const auto width = max - min;
        const auto offsetValue = value - min;

        return (offsetValue - (floor(offsetValue / width) * width)) + min;
    }

    glm::vec3 MathHelpers::normalizeSafely(const glm::vec3& vector) {
        const auto squareSum = vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;

        if (squareSum < smallFloatEpsilon) {
            return glm::vec3(0.0F, 0.0F, 0.0F);
        }

        return vector * glm::inversesqrt(squareSum);
    }
} // namespace ne
