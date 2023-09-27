#pragma once

// Standard.
#include <vector>

// Custom.
#include "math/GLMath.hpp"
#include "misc/shapes/Plane.h"

namespace ne {
    struct MeshVertex;

    /** Axis-aligned bounding box. */
    struct AABB {
        /**
         * Creates a new AABB from the specified vertices.
         *
         * @param pVertices Vertices to process.
         *
         * @return Created AABB.
         */
        static AABB createFromVertices(std::vector<MeshVertex>* pVertices);

        /**
         * Tests if this AABB intersects the specified plane or lays in front of the specified plane
         * (in the direction where plane's normal points).
         *
         * @param plane Plane to test.
         *
         * @return `true` if this AABB intersects the specified plane or lays in front of the specified plane,
         * `false` otherwise.
         */
        bool isIntersectsOrInFrontOfPlane(const Plane& plane);

        /** Center of the AABB in model space. */
        glm::vec3 center = glm::vec3(0.0F, 0.0F, 0.0F);

        /** Half extension (size) of the AABB in model space. */
        glm::vec3 extents = glm::vec3(0.0F, 0.0F, 0.0F);
    };
}
