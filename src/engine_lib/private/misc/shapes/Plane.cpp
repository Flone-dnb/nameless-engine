#include "misc/shapes/Plane.h"

namespace ne {

    Plane::Plane(const glm::vec3& normal, const glm::vec3& location) {
        this->normal = normal;
        distanceFromOrigin = glm::dot(normal, location);
    }

    bool Plane::isPointBehindPlane(const glm::vec3& point) const {
        // Source: Real-time collision detection, Christer Ericson (2005).
        return glm::dot(normal, point) - distanceFromOrigin < 0.0F;
    }

}
