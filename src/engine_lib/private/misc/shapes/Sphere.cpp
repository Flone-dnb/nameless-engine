#include "misc/shapes/Sphere.h"

namespace ne {

    Sphere::Sphere(const glm::vec3& center, float radius) {
        this->center = center;
        this->radius = radius;
    }

    bool Sphere::isBehindPlane(const Plane& plane) const {
        // Source: Real-time collision detection, Christer Ericson (2005).
        return glm::dot(plane.normal, center) - plane.distanceFromOrigin < -radius;
    }

}
