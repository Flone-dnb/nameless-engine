#pragma once

// Custom.
#include "math/GLMath.hpp"
#include "misc/shapes/Plane.h"

namespace ne {
    /** Sphere shape. */
    struct Sphere {
        /** Creates uninitialized sphere. */
        Sphere() = default;

        /**
         * Initializes the sphere.
         *
         * @param center Location of the sphere's center point.
         * @param radius Sphere's radius.
         */
        Sphere(const glm::vec3& center, float radius);

        /**
         * Tells if the sphere is fully behind (inside the negative halfspace of) a plane.
         *
         * @param plane Plane to test.
         *
         * @return `true` if the sphere is fully behind the plane, `false` if intersects or in front of it.
         */
        bool isBehindPlane(const Plane& plane) const;

        /** Location of the sphere's center point. */
        glm::vec3 center = glm::vec3(0.0F, 0.0F, 0.0F);

        /** Sphere's radius. */
        float radius = 1.0F;
    };
}
