#pragma once

// Standard.
#include <vector>

// Custom.
#include "math/GLMath.hpp"

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

        /** Minimum position of the box. */
        glm::vec3 min = glm::vec3(0.0F, 0.0F, 0.0F);

        /** Maximum position of the box. */
        glm::vec3 max = glm::vec3(0.0F, 0.0F, 0.0F);
    };
}
