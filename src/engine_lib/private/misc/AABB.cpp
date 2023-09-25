#include "misc/AABB.h"

// Standard.
#include <limits>

// Custom.
#include "game/nodes/MeshNode.h"

#undef max
#undef min

namespace ne {

    AABB AABB::createFromVertices(std::vector<MeshVertex>* pVertices) {
        AABB aabb;

        aabb.min = glm::vec3(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max());

        aabb.max = glm::vec3(
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max(),
            -std::numeric_limits<float>::max());

        for (size_t i = 0; i < pVertices->size(); i++) {
            auto& position = pVertices->at(i).position;

            aabb.min.x = std::min(aabb.min.x, position.x);
            aabb.min.y = std::min(aabb.min.y, position.y);
            aabb.min.z = std::min(aabb.min.z, position.z);

            aabb.max.x = std::max(aabb.max.x, position.x);
            aabb.max.y = std::max(aabb.max.y, position.y);
            aabb.max.z = std::max(aabb.max.z, position.z);
        }

        return aabb;
    }

}
