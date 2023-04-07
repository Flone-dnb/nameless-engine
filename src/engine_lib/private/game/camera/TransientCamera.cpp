#include "game/camera/TransientCamera.h"

// Custom.
#include "io/Logger.h"
#include "math/MathHelpers.hpp"

namespace ne {

    void TransientCamera::setFreeCameraForwardMovement(float input) {
        lastInputDirection.x = std::clamp(input, -1.0F, 1.0F);

        // Stop input processing if the input is zero.
        if (glm::all(glm::epsilonEqual(lastInputDirection, glm::vec3(0.0F, 0.0F, 0.0F), inputDelta))) {
            bHasInputToProcess = false;
            return;
        }

        // Normalize in order to avoid speed boost when multiple input keys are pressed.
        lastInputDirection = glm::normalize(lastInputDirection);

        // Mark input to be processed.
        bHasInputToProcess = true;
    }

    void TransientCamera::setFreeCameraRightMovement(float input) {
        lastInputDirection.y = std::clamp(input, -1.0F, 1.0F);

        // Stop input processing if the input is zero.
        if (glm::all(glm::epsilonEqual(lastInputDirection, glm::vec3(0.0F, 0.0F, 0.0F), inputDelta))) {
            bHasInputToProcess = false;
            return;
        }

        // Normalize in order to avoid speed boost when multiple input keys are pressed.
        lastInputDirection = glm::normalize(lastInputDirection);

        // Mark input to be processed.
        bHasInputToProcess = true;
    }

    void TransientCamera::setFreeCameraWorldUpMovement(float input) {
        lastInputDirection.z = std::clamp(input, -1.0F, 1.0F);

        // Stop input processing if the input is zero.
        if (glm::all(glm::epsilonEqual(lastInputDirection, glm::vec3(0.0F, 0.0F, 0.0F), inputDelta))) {
            bHasInputToProcess = false;
            return;
        }

        // Normalize in order to avoid speed boost when multiple input keys are pressed.
        lastInputDirection = glm::normalize(lastInputDirection);

        // Mark input to be processed.
        bHasInputToProcess = true;
    }

    void TransientCamera::setCameraMode(CameraMode mode) {
        std::scoped_lock guard(cameraProperties.mtxData.first);

        cameraProperties.mtxData.second.currentCameraMode = mode;

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.first = true;
    }

    void TransientCamera::setLocation(const glm::vec3& location) {
        std::scoped_lock guard(cameraProperties.mtxData.first);

        // Update camera properties.
        cameraProperties.mtxData.second.viewData.second.worldLocation = location;
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::ORBITAL) {
            // Calculate rotation based on new location.
            MathHelpers::convertCartesianCoordinatesToSpherical(
                cameraProperties.mtxData.second.viewData.second.worldLocation -
                    cameraProperties.mtxData.second.viewData.second.targetPointWorldLocation,
                cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
                cameraProperties.mtxData.second.orbitalModeData.theta,
                cameraProperties.mtxData.second.orbitalModeData.phi);

            recalculateBaseVectorsForOrbitalCamera();
        }

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.first = true;
    }

    void TransientCamera::setFreeCameraRotation(const glm::vec3& rotation) {
        std::scoped_lock guard(cameraProperties.mtxData.first);

        // Make sure we are in the free camera mode.
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set free camera rotation was ignored because the camera is not in "
                "the free mode",
                sTransientCameraLogCategory);
            return;
        }

        // Build rotation matrix.
        const auto rotationMatrix = MathHelpers::buildRotationMatrix(rotation);

        // Recalculate axis.
        cameraForwardDirection = rotationMatrix * glm::vec4(cameraForwardDirection, 0.0F);
        cameraRightDirection = rotationMatrix * glm::vec4(cameraRightDirection, 0.0F);
        cameraUpDirection = rotationMatrix * glm::vec4(cameraUpDirection, 0.0F);

        // Save new rotation.
        cameraRotation = rotation;

        // Update camera properties.
        cameraProperties.mtxData.second.viewData.second.targetPointWorldLocation =
            cameraProperties.mtxData.second.viewData.second.worldLocation + cameraForwardDirection;
        cameraProperties.mtxData.second.viewData.second.worldUpDirection = cameraUpDirection;

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.first = true;
    }

    void TransientCamera::setOrbitalCameraTargetLocation(const glm::vec3& targetLocation) {
        std::scoped_lock guard(cameraProperties.mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set orbital camera target location was ignored because the camera is not in "
                "the orbital mode",
                sTransientCameraLogCategory);
            return;
        }

        // Update camera properties.
        cameraProperties.mtxData.second.viewData.second.targetPointWorldLocation = targetLocation;

        // Calculate rotation based on new target point location.
        MathHelpers::convertCartesianCoordinatesToSpherical(
            cameraProperties.mtxData.second.viewData.second.worldLocation -
                cameraProperties.mtxData.second.viewData.second.targetPointWorldLocation,
            cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
            cameraProperties.mtxData.second.orbitalModeData.theta,
            cameraProperties.mtxData.second.orbitalModeData.phi);

        recalculateBaseVectorsForOrbitalCamera();

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.first = true;
    }

    void TransientCamera::setOrbitalCameraDistanceToTarget(float distanceToTarget) {
        std::scoped_lock guard(cameraProperties.mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set orbital camera distance to target was ignored because the camera is not "
                "in the orbital mode",
                sTransientCameraLogCategory);
            return;
        }

        // Apply distance.
        cameraProperties.mtxData.second.orbitalModeData.distanceToTarget = distanceToTarget;

        // Recalculate location.
        cameraProperties.mtxData.second.viewData.second.worldLocation =
            MathHelpers::convertSphericalToCartesianCoordinates(
                cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
                cameraProperties.mtxData.second.orbitalModeData.theta,
                cameraProperties.mtxData.second.orbitalModeData.phi) +
            cameraProperties.mtxData.second.viewData.second.targetPointWorldLocation;

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.first = true;
    }

    void TransientCamera::setOrbitalCameraRotation(float phi, float theta) {
        std::scoped_lock guard(cameraProperties.mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set orbital camera rotation was ignored because the camera is not "
                "in the orbital mode",
                sTransientCameraLogCategory);
            return;
        }

        // Apply rotation.
        cameraProperties.mtxData.second.orbitalModeData.phi = phi;
        cameraProperties.mtxData.second.orbitalModeData.theta = theta;

        // Recalculate location.
        cameraProperties.mtxData.second.viewData.second.worldLocation =
            MathHelpers::convertSphericalToCartesianCoordinates(
                cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
                cameraProperties.mtxData.second.orbitalModeData.theta,
                cameraProperties.mtxData.second.orbitalModeData.phi) +
            cameraProperties.mtxData.second.viewData.second.worldLocation;

        recalculateBaseVectorsForOrbitalCamera();

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.first = true;
    }

    glm::vec3 TransientCamera::getFreeCameraRotation() const { return cameraRotation; }

    CameraProperties* TransientCamera::getCameraProperties() { return &cameraProperties; }

    void TransientCamera::onBeforeNewFrame(float timeSincePrevCallInSec) {
        // Make sure the input is not zero.
        if (!bHasInputToProcess) {
            return;
        }

        moveFreeCamera(lastInputDirection * timeSincePrevCallInSec);
    }

    void TransientCamera::clearInput() { lastInputDirection = glm::vec3(0.0F, 0.0F, 0.0F); }

    void TransientCamera::moveFreeCamera(const glm::vec3& distance) {
        // Make sure the input is not zero.
        if (glm::all(glm::epsilonEqual(distance, glm::vec3(0.0F, 0.0F, 0.0F), inputDelta))) {
            return;
        }

        std::scoped_lock guard(cameraProperties.mtxData.first);

        // Make sure we are in the free camera mode.
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt to move free camera forward was ignored because the camera is not in "
                "the free mode",
                sTransientCameraLogCategory);
            return;
        }

        // Apply movement.
        cameraProperties.mtxData.second.viewData.second.worldLocation += cameraForwardDirection * distance.x;
        cameraProperties.mtxData.second.viewData.second.worldLocation += cameraRightDirection * distance.y;
        cameraProperties.mtxData.second.viewData.second.worldLocation += cameraUpDirection * distance.z;

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.first = true;
    }

} // namespace ne
