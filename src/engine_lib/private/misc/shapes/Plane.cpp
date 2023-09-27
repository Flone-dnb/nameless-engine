#include "misc/shapes/Plane.h"

namespace ne {

    Plane::Plane(const glm::vec3& normal, const glm::vec3& location) {
        this->normal = normal;
        distanceFromOrigin = glm::dot(normal, location);
    }

}
