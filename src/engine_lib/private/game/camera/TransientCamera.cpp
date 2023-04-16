#include "game/camera/TransientCamera.h"

// Custom.
#include "io/Logger.h"

namespace ne {

    void TransientCamera::setFreeCameraForwardMovement(float input) {
        lastInputDirection.x = std::clamp(input, -1.0F, 1.0F);
    }

    void TransientCamera::setFreeCameraRightMovement(float input) {
        lastInputDirection.y = std::clamp(input, -1.0F, 1.0F);
    }

    void TransientCamera::setFreeCameraWorldUpMovement(float input) {
        lastInputDirection.z = std::clamp(input, -1.0F, 1.0F);
    }

    void TransientCamera::setCameraMode(CameraMode mode) {
        std::scoped_lock guard(cameraProperties.mtxData.first);

        cameraProperties.mtxData.second.currentCameraMode = mode;

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.bViewMatrixNeedsUpdate = true;
    }

    void TransientCamera::setLocation(const glm::vec3& location) {
        std::scoped_lock guard(cameraProperties.mtxData.first);

        // Update camera properties.
        cameraProperties.mtxData.second.viewData.worldLocation = location;
        if (cameraProperties.mtxData.second.currentCameraMode == CameraMode::ORBITAL) {
            // Calculate rotation based on new location.
            MathHelpers::convertCartesianCoordinatesToSpherical(
                cameraProperties.mtxData.second.viewData.worldLocation -
                    cameraProperties.mtxData.second.viewData.targetPointWorldLocation,
                cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
                cameraProperties.mtxData.second.orbitalModeData.theta,
                cameraProperties.mtxData.second.orbitalModeData.phi);

            recalculateBaseVectorsForOrbitalCamera();
        }

        // Update target.
        cameraProperties.mtxData.second.viewData.targetPointWorldLocation =
            cameraProperties.mtxData.second.viewData.worldLocation + cameraForwardDirection;

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.bViewMatrixNeedsUpdate = true;
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

        // Save new rotation.
        cameraRotation.x = MathHelpers::normalizeValue(rotation.x, -360.0F, 360.0F); // NOLINT
        cameraRotation.y = MathHelpers::normalizeValue(rotation.y, -360.0F, 360.0F); // NOLINT
        cameraRotation.z = MathHelpers::normalizeValue(rotation.z, -360.0F, 360.0F); // NOLINT

        // Build rotation matrix.
        const auto rotationMatrix = MathHelpers::buildRotationMatrix(cameraRotation);

        // Recalculate axis.
        cameraForwardDirection = rotationMatrix * glm::vec4(Globals::WorldDirection::forward, 0.0F);
        cameraRightDirection = rotationMatrix * glm::vec4(Globals::WorldDirection::right, 0.0F);
        cameraUpDirection = rotationMatrix * glm::vec4(Globals::WorldDirection::up, 0.0F);

        // Update camera properties.
        cameraProperties.mtxData.second.viewData.targetPointWorldLocation =
            cameraProperties.mtxData.second.viewData.worldLocation + cameraForwardDirection;
        cameraProperties.mtxData.second.viewData.worldUpDirection = cameraUpDirection;

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.bViewMatrixNeedsUpdate = true;
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
        cameraProperties.mtxData.second.viewData.targetPointWorldLocation = targetLocation;

        // Calculate rotation based on new target point location.
        MathHelpers::convertCartesianCoordinatesToSpherical(
            cameraProperties.mtxData.second.viewData.worldLocation -
                cameraProperties.mtxData.second.viewData.targetPointWorldLocation,
            cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
            cameraProperties.mtxData.second.orbitalModeData.theta,
            cameraProperties.mtxData.second.orbitalModeData.phi);

        recalculateBaseVectorsForOrbitalCamera();

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.bViewMatrixNeedsUpdate = true;
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
        cameraProperties.mtxData.second.viewData.worldLocation =
            MathHelpers::convertSphericalToCartesianCoordinates(
                cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
                cameraProperties.mtxData.second.orbitalModeData.theta,
                cameraProperties.mtxData.second.orbitalModeData.phi) +
            cameraProperties.mtxData.second.viewData.targetPointWorldLocation;

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.bViewMatrixNeedsUpdate = true;
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
        cameraProperties.mtxData.second.viewData.worldLocation =
            MathHelpers::convertSphericalToCartesianCoordinates(
                cameraProperties.mtxData.second.orbitalModeData.distanceToTarget,
                cameraProperties.mtxData.second.orbitalModeData.theta,
                cameraProperties.mtxData.second.orbitalModeData.phi) +
            cameraProperties.mtxData.second.viewData.worldLocation;

        recalculateBaseVectorsForOrbitalCamera();

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.bViewMatrixNeedsUpdate = true;
    }

    void TransientCamera::setCameraMovementSpeed(float speed) { cameraMovementSpeed = speed; }

    glm::vec3 TransientCamera::getFreeCameraRotation() const { return cameraRotation; }

    CameraProperties* TransientCamera::getCameraProperties() { return &cameraProperties; }

    void TransientCamera::onBeforeNewFrame(float timeSincePrevCallInSec) {
        // Make sure the input is not zero.
        if (glm::all(glm::epsilonEqual(lastInputDirection, glm::vec3(0.0F, 0.0F, 0.0F), inputDelta))) {
            return;
        }

        // Normalize in order to avoid speed boost when multiple input keys are pressed.
        moveFreeCamera(glm::normalize(lastInputDirection) * timeSincePrevCallInSec * cameraMovementSpeed);
    }

    void TransientCamera::clearInput() { lastInputDirection = glm::vec3(0.0F, 0.0F, 0.0F); }

    void TransientCamera::moveFreeCamera(const glm::vec3& distance) {
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
        cameraProperties.mtxData.second.viewData.worldLocation += cameraForwardDirection * distance.x;
        cameraProperties.mtxData.second.viewData.worldLocation += cameraRightDirection * distance.y;
        cameraProperties.mtxData.second.viewData.worldLocation += cameraUpDirection * distance.z;

        // Recalculate look direction.
        cameraProperties.mtxData.second.viewData.targetPointWorldLocation =
            cameraProperties.mtxData.second.viewData.worldLocation + cameraForwardDirection;

        // Mark view matrix as "needs update".
        cameraProperties.mtxData.second.viewData.bViewMatrixNeedsUpdate = true;
    }

} // namespace ne
