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
         * Tells if the AABB is fully behind (inside the negative halfspace of) a plane.
         *
         * @param plane Plane to test.
         *
         * @return `true` if the AABB is fully behind the plane, `false` if intersects or in front of it.
         */
        bool isBehindPlane(const Plane& plane) const;

        /** Center of the AABB in model space. */
        glm::vec3 center = glm::vec3(0.0F, 0.0F, 0.0F);

        /** Half extension (size) of the AABB in model space. */
        glm::vec3 extents = glm::vec3(0.0F, 0.0F, 0.0F);
    };
}
