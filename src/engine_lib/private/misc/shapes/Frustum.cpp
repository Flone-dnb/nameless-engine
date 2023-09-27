#include "misc/shapes/Frustum.h"

// Custom.
#include "misc/Globals.h"

namespace ne {

    bool Frustum::isAabbInFrustum(const AABB& aabbInModelSpace, const glm::mat4x4& worldMatrix) const {
        // Before comparing frustum faces against AABB we need to take care of something:
        // we can't just transform AABB to world space (using world matrix) as this would result
        // in OBB (oriented bounding box) because of rotation in world matrix while we need an AABB.

        // Prepare an AABB that stores OBB (in world space) converted to AABB (in world space).
        AABB aabb;

        // Calculate AABB center.
        aabb.center = worldMatrix * glm::vec4(aabbInModelSpace.center, 1.0F);

        // Calculate OBB directions in world space
        // (directions are considered to point from OBB's center).
        const glm::vec3 obbScaledForward =
            worldMatrix * glm::vec4(Globals::WorldDirection::forward, 0.0F) * aabbInModelSpace.extents.x;
        const glm::vec3 obbScaledRight =
            worldMatrix * glm::vec4(Globals::WorldDirection::right, 0.0F) * aabbInModelSpace.extents.y;
        const glm::vec3 obbScaledUp =
            worldMatrix * glm::vec4(Globals::WorldDirection::up, 0.0F) * aabbInModelSpace.extents.z;

        // If the specified world matrix contained a rotation OBB's directions are no longer aligned
        // with world axes. We need to adjust these OBB directions to be world axis aligned and save them
        // as resulting AABB extents.

        // We can convert scaled OBB directions to AABB extents (directions) by projecting each OBB direction
        // onto world axis.

        // Calculate X extent.
        aabb.extents.x =
            std::abs(glm::dot(obbScaledForward, glm::vec3(1.0F, 0.0F, 0.0F))) + // project OBB X on world X
            std::abs(glm::dot(obbScaledRight, glm::vec3(1.0F, 0.0F, 0.0F))) +   // project OBB Y on world X
            std::abs(glm::dot(obbScaledUp, glm::vec3(1.0F, 0.0F, 0.0F)));       // project OBB Z on world X

        // Calculate Y extent.
        aabb.extents.y =
            std::abs(glm::dot(obbScaledForward, glm::vec3(0.0F, 1.0F, 0.0F))) + // project OBB X on world Y
            std::abs(glm::dot(obbScaledRight, glm::vec3(0.0F, 1.0F, 0.0F))) +   // project OBB Y on world Y
            std::abs(glm::dot(obbScaledUp, glm::vec3(0.0F, 1.0F, 0.0F)));       // project OBB Z on world Y

        // Calculate Z extent.
        aabb.extents.z =
            std::abs(glm::dot(obbScaledForward, glm::vec3(0.0F, 0.0F, 1.0F))) + // project OBB X on world Z
            std::abs(glm::dot(obbScaledRight, glm::vec3(0.0F, 0.0F, 1.0F))) +   // project OBB Y on world Z
            std::abs(glm::dot(obbScaledUp, glm::vec3(0.0F, 0.0F, 1.0F)));       // project OBB Z on world Z

        // Test each AABB face against the frustum.
        return aabb.isIntersectsOrInFrontOfPlane(leftFace) && aabb.isIntersectsOrInFrontOfPlane(rightFace) &&
               aabb.isIntersectsOrInFrontOfPlane(topFace) && aabb.isIntersectsOrInFrontOfPlane(bottomFace) &&
               aabb.isIntersectsOrInFrontOfPlane(nearFace) && aabb.isIntersectsOrInFrontOfPlane(farFace);
    }

}
