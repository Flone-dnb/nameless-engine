#pragma once

// Custom.
#include "math/GLMath.hpp"
#include "misc/shapes/Plane.h"

namespace ne {
    /** Cone shape. */
    struct Cone {
        /** Creates uninitialized cone. */
        Cone() = default;

        /**
         * Initializes the cone.
         *
         * @param location     Location of cone's tip.
         * @param height       Height of the cone.
         * @param direction    Direction unit vector from cone's tip.
         * @param bottomRadius Radius of the bottom part of the cone.
         */
        Cone(const glm::vec3& location, float height, const glm::vec3& direction, float bottomRadius);

        /**
         * Tells if the cone is fully behind (inside the negative halfspace of) a plane.
         *
         * @param plane Plane to test.
         *
         * @return `true` if the cone is fully behind the plane, `false` if intersects or in front of it.
         */
        bool isBehindPlane(const Plane& plane) const;

        /** Location of cone's tip. */
        glm::vec3 location = glm::vec3(0.0F, 0.0F, 0.0F);

        /** Height of the cone. */
        float height = 1.0F;

        /** Direction unit vector from cone's tip. */
        glm::vec3 direction = glm::vec3(1.0F, 0.0F, 0.0F);

        /** Radius of the bottom part of the cone. */
        float bottomRadius = 1.0F;
    };
}
