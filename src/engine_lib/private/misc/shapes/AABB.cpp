#include "misc/shapes/AABB.h"

// Standard.
#include <limits>

// Custom.
#include "game/nodes/MeshNode.h"

#undef max
#undef min

namespace ne {

    AABB AABB::createFromVertices(std::vector<MeshVertex>* pVertices) {
        // Prepare variables to store the minimum and the maximum positions of the AABB in model space.
        auto min = glm::vec3(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max());

        auto max = glm::vec3(
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max());

        // Iterate over all vertices and update min/max of the AABB according to the vertex positions.
        for (size_t i = 0; i < pVertices->size(); i++) {
            auto& position = pVertices->at(i).position;

            min.x = std::min(min.x, position.x);
            min.y = std::min(min.y, position.y);
            min.z = std::min(min.z, position.z);

            max.x = std::max(max.x, position.x);
            max.y = std::max(max.y, position.y);
            max.z = std::max(max.z, position.z);
        }

        // Create the final AABB.
        AABB aabb;
        aabb.center =
            glm::vec3((min.x + max.x) * 0.5F, (min.y + max.y) * 0.5F, (min.z + max.z) * 0.5F); // NOLINT
        aabb.extents = glm::vec3(max.x - aabb.center.x, max.y - aabb.center.y, max.z - aabb.center.z);

        return aabb;
    }

    bool AABB::isIntersectsOrInFrontOfPlane(const Plane& plane) const {
        // Source: https://github.com/gdbooks/3DCollisions/blob/master/Chapter2/static_aabb_plane.md

        const float projectionRadius = extents.x * std::abs(plane.normal.x) +
                                       extents.y * std::abs(plane.normal.y) +
                                       extents.z * std::abs(plane.normal.z);

        const auto distanceToPlane = glm::dot(plane.normal, center) - plane.distanceFromOrigin;

        return -projectionRadius <= distanceToPlane;
    }

}
