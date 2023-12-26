#pragma once

// Custom.
#include "math/GLMath.hpp"

namespace ne {
    /** Plane represented by a normal and a distance. */
    struct Plane {
        /** Creates uninitialized plane. */
        Plane() = default;

        /**
         * Initializes the plane.
         *
         * @param normal   Plane's normal (expected to be of unit length).
         * @param location Location of a point that lies on the plane.
         */
        Plane(const glm::vec3& normal, const glm::vec3& location);

        /**
         * Tells if the point is fully behind (inside the negative halfspace of) a plane.
         *
         * @param point Point to test.
         *
         * @return `true` if the point is fully behind the plane, `false` otherwise.
         */
        bool isPointBehindPlane(const glm::vec3& point) const;

        /** Plane's normal. */
        glm::vec3 normal = glm::vec3(0.0F, 0.0F, 0.0F);

        /** Distance from the origin to the nearest point on the plane. */
        float distanceFromOrigin = 0.0F;
    };
}
