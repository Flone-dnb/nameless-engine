#include "game/nodes/CameraNode.h"

// Custom.
#include "game/GameInstance.h"
#include "game/camera/CameraManager.h"
#include "math/MathHelpers.hpp"

#include "CameraNode.generated_impl.h"

namespace ne {

    CameraNode::CameraNode() : CameraNode("Camera Node") {}

    CameraNode::CameraNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
        mtxIsActive.second = false;
    }

    void CameraNode::onWorldLocationRotationScaleChanged() {
        SpatialNode::onWorldLocationRotationScaleChanged();

        {
            // Update origin location in world space.
            const auto pMtxSpatialParent = getClosestSpatialParent();
            std::scoped_lock spatialParentGuard(pMtxSpatialParent->first);

            glm::mat4x4 parentWorldMatrix = glm::identity<glm::mat4x4>();
            if (pMtxSpatialParent->second != nullptr) {
                parentWorldMatrix = pMtxSpatialParent->second->getWorldMatrix();
            }

            localSpaceOriginInWorldSpace = parentWorldMatrix * glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);
        }

        updateCameraProperties();
    }

    CameraProperties* CameraNode::getCameraProperties() { return &cameraProperties; }

    void CameraNode::onDespawning() {
        SpatialNode::onDespawning();

        {
            std::scoped_lock guardActive(mtxIsActive.first);
            if (mtxIsActive.second) {
                // Notify camera manager.
                getGameInstance()->getCameraManager()->onCameraNodeDespawning(this);
            }
        }
    }

    void CameraNode::updateCameraProperties() {
        // Update camera properties.
        std::scoped_lock cameraPropertiesGuard(cameraProperties.mtxData.first);

        CameraProperties::Data::ViewData& viewData = cameraProperties.mtxData.second.viewData.second;

        viewData.worldLocation = getWorldLocation();

        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::FREE) {
            // Update free camera properties.
            viewData.targetPointWorldLocation = viewData.worldLocation + getWorldForwardDirection();
            viewData.worldUpDirection = getWorldUpDirection();
        } else {
            // Update orbital camera properties.
            if (orbitalCameraTargetInWorldSpace.has_value()) {
                viewData.targetPointWorldLocation = orbitalCameraTargetInWorldSpace.value();
            } else {
                viewData.targetPointWorldLocation = localSpaceOriginInWorldSpace;
            }

            // Update rotation.
            MathHelpers::convertCartesianCoordinatesToSpherical(
                viewData.worldLocation - viewData.targetPointWorldLocation,
                cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
                cameraProperties.mtxData.second.orbitalModeData.theta,
                cameraProperties.mtxData.second.orbitalModeData.phi);

            // Make node look at target.
            const auto toTarget = viewData.targetPointWorldLocation - viewData.worldLocation;
            glm::vec3 targetRotation =
                MathHelpers::convertDirectionToRollPitchYaw(MathHelpers::normalizeSafely(toTarget));

            // Set rotation if different.
            if (!glm::all(glm::epsilonEqual(targetRotation, getWorldRotation(), rotationDelta))) {
                setWorldRotation(targetRotation);
            }
        }

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.first = true;
    }

    void CameraNode::setCameraMode(CameraMode mode) {
        // Update camera properties.
        std::scoped_lock cameraPropertiesGuard(cameraProperties.mtxData.first);

        cameraProperties.mtxData.second.currentCameraMode = mode;

        updateCameraProperties();
    }

    void CameraNode::clearOrbitalTargetLocation() {
        // Update camera properties.
        std::scoped_lock cameraPropertiesGuard(cameraProperties.mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to clear orbital camera's target location was ignored because the camera is not "
                "in the orbital mode",
                sCameraNodeLogCategory);
            return;
        }

        orbitalCameraTargetInWorldSpace = {};

        updateCameraProperties();
    }

    void CameraNode::setOrbitalTargetLocation(const glm::vec3& targetPointLocation) {
        // Update camera properties.
        std::scoped_lock cameraPropertiesGuard(cameraProperties.mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set orbital camera's target location was ignored because the camera is not in "
                "the orbital mode",
                sCameraNodeLogCategory);
            return;
        }

        orbitalCameraTargetInWorldSpace = targetPointLocation;

        updateCameraProperties();
    }

    void CameraNode::setOrbitalRotation(float phi, float theta) {
        // Update camera properties.
        std::scoped_lock cameraPropertiesGuard(cameraProperties.mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set orbital camera's rotation was ignored because the camera is not in "
                "the orbital mode",
                sCameraNodeLogCategory);
            return;
        }

        // Update properties.
        cameraProperties.mtxData.second.orbitalModeData.phi = phi;
        cameraProperties.mtxData.second.orbitalModeData.theta = theta;

        // Change node's location according to new spherical rotation.
        const auto newWorldLocation =
            MathHelpers::convertSphericalToCartesianCoordinates(
                cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
                cameraProperties.mtxData.second.orbitalModeData.theta,
                cameraProperties.mtxData.second.orbitalModeData.phi) +
            cameraProperties.mtxData.second.viewData.second.targetPointWorldLocation;

        setWorldLocation(newWorldLocation); // causes `updateCameraProperties` to be called
    }

    void CameraNode::setOrbitalDistanceToTarget(float distanceToTarget) {
        // Update camera properties.
        std::scoped_lock cameraPropertiesGuard(cameraProperties.mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set orbital camera's rotation was ignored because the camera is not in "
                "the orbital mode",
                sCameraNodeLogCategory);
            return;
        }

        // Update properties.
        cameraProperties.mtxData.second.orbitalModeData.distanceToTarget = distanceToTarget;

        // Change node's location according to new spherical rotation.
        const auto newWorldLocation =
            MathHelpers::convertSphericalToCartesianCoordinates(
                cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
                cameraProperties.mtxData.second.orbitalModeData.theta,
                cameraProperties.mtxData.second.orbitalModeData.phi) +
            cameraProperties.mtxData.second.viewData.second.targetPointWorldLocation;

        setWorldLocation(newWorldLocation); // causes `updateCameraProperties` to be called
    }

    glm::vec3 CameraNode::getOrbitalTargetLocation() {
        std::scoped_lock cameraPropertiesGuard(cameraProperties.mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to get orbital camera's target location was ignored because the camera is not in "
                "the orbital mode",
                sCameraNodeLogCategory);
            return glm::vec3(0.0F, 0.0F, 0.0F);
        }

        if (orbitalCameraTargetInWorldSpace.has_value()) {
            return orbitalCameraTargetInWorldSpace.value();
        }

        return localSpaceOriginInWorldSpace;
    }

} // namespace ne
