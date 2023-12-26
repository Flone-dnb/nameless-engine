#include "misc/shapes/Cone.h"

namespace ne {

    Cone::Cone(const glm::vec3& location, float height, const glm::vec3& direction, float bottomRadius) {
        this->location = location;
        this->height = height;
        this->direction = direction;
        this->bottomRadius = bottomRadius;
    }

    bool Cone::isBehindPlane(const Plane& plane) const {
        // Source: Real-time collision detection, Christer Ericson (2005).

        // Calculate an intermediate vector which is parallel but opposite to plane's normal and perpendicular
        // to the cone's direction.
        glm::vec3 m = glm::cross(glm::cross(plane.normal, direction), direction);

        // Calculate the point Q that is on the base (bottom) of the cone that is farthest away from the plane
        // in the direction of plane's normal.
        glm::vec3 Q = location + direction * height - m * bottomRadius;

        // The cone is behind the plane if both cone's tip and Q are behind the plane.
        return plane.isPointBehindPlane(location) && plane.isPointBehindPlane(Q);
    }

}
