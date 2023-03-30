#include "game/camera/CameraProperties.h"

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"
#include "math/MathHelpers.hpp"

// External.
#include "fmt/core.h"

namespace ne {

    CameraProperties::CameraProperties() {
        makeSureViewMatrixIsUpToDate();
        makeSureProjectionMatrixAndClipPlanesAreUpToDate();
    }

    void CameraProperties::setAspectRatio(int iRenderTargetWidth, int iRenderTargetHeight) {
        std::scoped_lock guard(mtxData.first);

        // Apply change.
        mtxData.second.projectionData.second.iRenderTargetWidth = iRenderTargetWidth;
        mtxData.second.projectionData.second.iRenderTargetHeight = iRenderTargetHeight;

        // Mark projection matrix as "needs update".
        mtxData.second.projectionData.first = true;
    }

    void CameraProperties::setCameraMode(CameraMode cameraMode) {
        std::scoped_lock guard(mtxData.first);

        if (cameraMode == mtxData.second.currentCameraMode) {
            return;
        }

        // Apply mode.
        mtxData.second.currentCameraMode = cameraMode;

        if (mtxData.second.currentCameraMode == CameraMode::FREE) {
            // Reset rotation because in orbital mode pitch/yaw/roll were ignored and they are now invalid.
            mtxData.second.forwardDirection = worldForwardDirection;
            mtxData.second.rightDirection = worldRightDirection;
            mtxData.second.upDirection = worldUpDirection;
            mtxData.second.freeModeData.rotation = glm::vec3(0.0F, 0.0F, 0.0F);
        }

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::setCameraLocation(const glm::vec3& location) {
        std::scoped_lock guard(mtxData.first);

        // Apply camera location.
        mtxData.second.location = location;

        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) {
            // Calculate rotation based on new camera location.
            MathHelpers::convertCartesianCoordinatesToSpherical(
                mtxData.second.location - mtxData.second.orbitalModeData.targetPointLocation,
                mtxData.second.orbitalModeData.distanceToTarget,
                mtxData.second.orbitalModeData.theta,
                mtxData.second.orbitalModeData.phi);

            recalculateBaseVectorsForOrbitalCamera();
        }

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::moveFreeCameraForward(float distance) {
        // Make sure the input is not zero.
        if (glm::epsilonEqual(distance, 0.0F, floatDelta)) {
            return;
        }

        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt to move free camera forward was ignored because the camera is not in "
                "the free mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Apply movement.
        mtxData.second.location += mtxData.second.forwardDirection * distance;

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::moveFreeCameraRight(float distance) {
        // Make sure the input is not zero.
        if (glm::epsilonEqual(distance, 0.0F, floatDelta)) {
            return;
        }

        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt to move free camera right was ignored because the camera is not in "
                "the free mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Apply movement.
        mtxData.second.location += mtxData.second.rightDirection * distance;

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::moveFreeCameraUp(float distance) {
        // Make sure the input is not zero.
        if (glm::epsilonEqual(distance, 0.0F, floatDelta)) {
            return;
        }

        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt to move free camera up was ignored because the camera is not in "
                "the free mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Apply movement.
        mtxData.second.location += mtxData.second.upDirection * distance;

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::moveFreeCameraWorldUp(float distance) {
        // Make sure the input is not zero.
        if (glm::epsilonEqual(distance, 0.0F, floatDelta)) {
            return;
        }

        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt to move free camera up was ignored because the camera is not in "
                "the free mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Apply movement.
        mtxData.second.location += worldUpDirection * distance;

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::setFreeCameraPitch(float pitch) {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set free camera's pitch was ignored because the camera is not in "
                "the free mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Clamp pitch if should not flip the camera.
        if (mtxData.second.bDontFlipCamera) {
            pitch = std::clamp(pitch, -90.0F, 90.0F); // NOLINT: magic numbers
        }

        // Calculate diff.
        const auto pitchDiff = pitch - mtxData.second.freeModeData.rotation.y;

        // Build a rotation matrix around right direction.
        const auto rotationMatrix = glm::rotate(glm::radians(pitchDiff), mtxData.second.rightDirection);

        // Rotate forward/up vectors.
        mtxData.second.forwardDirection = rotationMatrix * glm::vec4(mtxData.second.forwardDirection, 0.0F);
        mtxData.second.upDirection = rotationMatrix * glm::vec4(mtxData.second.upDirection, 0.0F);

        // Save new pitch.
        mtxData.second.freeModeData.rotation.y = pitch;

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::setFreeCameraYaw(float yaw) {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set free camera's yaw was ignored because the camera is not in "
                "the free mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Calculate diff.
        const auto yawDiff = yaw - mtxData.second.freeModeData.rotation.z;

        // Build a rotation matrix around up direction.
        const auto rotationMatrix = glm::rotate(glm::radians(yawDiff), mtxData.second.upDirection);

        // Rotate forward/right vectors.
        mtxData.second.forwardDirection = rotationMatrix * glm::vec4(mtxData.second.forwardDirection, 0.0F);
        mtxData.second.rightDirection = rotationMatrix * glm::vec4(mtxData.second.rightDirection, 0.0F);

        // Save new yaw.
        mtxData.second.freeModeData.rotation.z = yaw;

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::setFreeCameraRoll(float roll) {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set free camera's roll was ignored because the camera is not in "
                "the free mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Make sure "don't flip the camera" is disabled.
        if (mtxData.second.bDontFlipCamera) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set free camera's roll was ignored because \"don't flip the camera\" mode is "
                "enabled",
                sCameraPropertiesLogCategory);
            return;
        }

        // Calculate diff.
        const auto rollDiff = roll - mtxData.second.freeModeData.rotation.x;

        // Build a rotation matrix around up direction.
        const auto rotationMatrix = glm::rotate(glm::radians(rollDiff), mtxData.second.forwardDirection);

        // Rotate right/up vectors.
        mtxData.second.rightDirection = rotationMatrix * glm::vec4(mtxData.second.rightDirection, 0.0F);
        mtxData.second.upDirection = rotationMatrix * glm::vec4(mtxData.second.upDirection, 0.0F);

        // Save new roll.
        mtxData.second.freeModeData.rotation.x = roll;

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::makeFreeCameraLookAt(const glm::vec3& targetLocation) {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt to make free camera look at was ignored because the camera is not in "
                "the free mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Recalculate base vectors.
        mtxData.second.forwardDirection = glm::normalize(targetLocation - mtxData.second.location);
        mtxData.second.rightDirection =
            glm::normalize(glm::cross(mtxData.second.forwardDirection, worldUpDirection));
        mtxData.second.upDirection =
            glm::cross(mtxData.second.rightDirection, mtxData.second.forwardDirection);

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::setOrbitalCameraTargetPoint(const glm::vec3& targetPointLocation) {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set orbital camera's target point was ignored because the camera is not in "
                "the orbital mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Apply target location.
        mtxData.second.orbitalModeData.targetPointLocation = targetPointLocation;

        // Calculate rotation based on new target point location.
        MathHelpers::convertCartesianCoordinatesToSpherical(
            mtxData.second.location - mtxData.second.orbitalModeData.targetPointLocation,
            mtxData.second.orbitalModeData.distanceToTarget,
            mtxData.second.orbitalModeData.theta,
            mtxData.second.orbitalModeData.phi);

        recalculateBaseVectorsForOrbitalCamera();

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::setOrbitalCameraDistanceToTarget(float distanceToTarget) {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set orbital camera's distance to target point was ignored because the camera "
                "is not in the orbital mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Apply distance.
        mtxData.second.orbitalModeData.distanceToTarget = distanceToTarget;

        // Recalculate location.
        mtxData.second.location = MathHelpers::convertSphericalToCartesianCoordinates(
                                      mtxData.second.orbitalModeData.distanceToTarget,
                                      mtxData.second.orbitalModeData.theta,
                                      mtxData.second.orbitalModeData.phi) +
                                  mtxData.second.orbitalModeData.targetPointLocation;

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::setOrbitalCameraRotation(float phi, float theta) {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt to set orbital camera's rotation was ignored because the camera is not in "
                "the orbital mode",
                sCameraPropertiesLogCategory);
            return;
        }

        // Apply rotation.
        mtxData.second.orbitalModeData.phi = phi;
        mtxData.second.orbitalModeData.theta = theta;

        // Recalculate location.
        mtxData.second.location = MathHelpers::convertSphericalToCartesianCoordinates(
                                      mtxData.second.orbitalModeData.distanceToTarget,
                                      mtxData.second.orbitalModeData.theta,
                                      mtxData.second.orbitalModeData.phi) +
                                  mtxData.second.orbitalModeData.targetPointLocation;

        recalculateBaseVectorsForOrbitalCamera();

        // Mark view matrix as "needs update".
        mtxData.second.viewMatrix.first = true;
    }

    void CameraProperties::setDontFlipCamera(bool bDontFlipCamera) {
        std::scoped_lock guard(mtxData.first);

        mtxData.second.bDontFlipCamera = bDontFlipCamera;
    }

    void CameraProperties::setFov(unsigned int iVerticalFov) {
        std::scoped_lock guard(mtxData.first);

        // Apply FOV.
        mtxData.second.projectionData.second.iVerticalFov = iVerticalFov;

        // Mark projection matrix as "needs update".
        mtxData.second.projectionData.first = true;
    }

    void CameraProperties::setNearClipPlaneDistance(float nearClipPlaneDistance) {
        if (nearClipPlaneDistance < CameraProperties::Data::minimumClipPlaneDistance) [[unlikely]] {
            Error error(fmt::format(
                "the specified near clip plane distance {} is lower than the minimum allowed clip plane "
                "distance: {}",
                nearClipPlaneDistance,
                CameraProperties::Data::minimumClipPlaneDistance));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        std::scoped_lock guard(mtxData.first);

        // Apply near clip plane.
        mtxData.second.projectionData.second.nearClipPlaneDistance = nearClipPlaneDistance;

        // Mark projection matrix as "needs update".
        mtxData.second.projectionData.first = true;
    }

    void CameraProperties::setFarClipPlaneDistance(float farClipPlaneDistance) {
        if (farClipPlaneDistance < CameraProperties::Data::minimumClipPlaneDistance) [[unlikely]] {
            Error error(fmt::format(
                "the specified far clip plane distance {} is lower than the minimum allowed clip plane "
                "distance: {}",
                farClipPlaneDistance,
                CameraProperties::Data::minimumClipPlaneDistance));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        std::scoped_lock guard(mtxData.first);

        // Apply far clip plane.
        mtxData.second.projectionData.second.farClipPlaneDistance = farClipPlaneDistance;

        // Mark projection matrix as "needs update".
        mtxData.second.projectionData.first = true;
    }

    glm::vec3 CameraProperties::getLocation(bool bInWorldSpace) {
        std::scoped_lock guard(mtxData.first);

        if (bInWorldSpace) {
            return mtxData.second.worldAdjustmentMatrix * glm::vec4(mtxData.second.location, 1.0F);
        }

        return mtxData.second.location;
    }

    glm::vec3 CameraProperties::getForwardDirection(bool bInWorldSpace) {
        std::scoped_lock guard(mtxData.first);

        if (bInWorldSpace) {
            return glm::normalize(
                mtxData.second.worldAdjustmentMatrix * glm::vec4(mtxData.second.forwardDirection, 0.0F));
        }

        return mtxData.second.forwardDirection;
    }

    glm::vec3 CameraProperties::getRightDirection(bool bInWorldSpace) {
        std::scoped_lock guard(mtxData.first);

        if (bInWorldSpace) {
            return glm::normalize(
                mtxData.second.worldAdjustmentMatrix * glm::vec4(mtxData.second.rightDirection, 0.0F));
        }

        return mtxData.second.rightDirection;
    }

    glm::vec3 CameraProperties::getUpDirection(bool bInWorldSpace) {
        std::scoped_lock guard(mtxData.first);

        if (bInWorldSpace) {
            return glm::normalize(
                mtxData.second.worldAdjustmentMatrix * glm::vec4(mtxData.second.upDirection, 0.0F));
        }

        return mtxData.second.upDirection;
    }

    glm::vec3 CameraProperties::getOrbitalCameraTargetLocation(bool bInWorldSpace) {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt was made to get orbital camera's target location while the camera is not "
                "in the orbital mode",
                sCameraPropertiesLogCategory);
            return glm::vec3(0.0F, 0.0F, 0.0F);
        }

        if (bInWorldSpace) {
            return mtxData.second.worldAdjustmentMatrix *
                   glm::vec4(mtxData.second.orbitalModeData.targetPointLocation, 1.0F);
        }

        return mtxData.second.orbitalModeData.targetPointLocation;
    }

    float CameraProperties::getFreeCameraPitch() {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt was made to get free camera's pitch while the camera is not "
                "in the free mode",
                sCameraPropertiesLogCategory);
            return 0.0F;
        }

        return mtxData.second.freeModeData.rotation.y;
    }

    float CameraProperties::getFreeCameraYaw() {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt was made to get free camera's yaw while the camera is not "
                "in the free mode",
                sCameraPropertiesLogCategory);
            return 0.0F;
        }

        return mtxData.second.freeModeData.rotation.z;
    }

    float CameraProperties::getFreeCameraRoll() {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the free camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::ORBITAL) [[unlikely]] {
            Logger::get().warn(
                "an attempt was made to get free camera's roll while the camera is not "
                "in the free mode",
                sCameraPropertiesLogCategory);
            return 0.0F;
        }

        return mtxData.second.freeModeData.rotation.x;
    }

    unsigned int CameraProperties::getVerticalFov() {
        std::scoped_lock guard(mtxData.first);

        return mtxData.second.projectionData.second.iVerticalFov;
    }

    float CameraProperties::getOrbitalCameraDistanceToTarget() {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt was made to get orbital camera's distance to target while the camera is not "
                "in the orbital mode",
                sCameraPropertiesLogCategory);
        }

        return mtxData.second.orbitalModeData.distanceToTarget;
    }

    float CameraProperties::getOrbitalCameraTheta() {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt was made to get orbital camera's theta while the camera is not "
                "in the orbital mode",
                sCameraPropertiesLogCategory);
        }

        return mtxData.second.orbitalModeData.theta;
    }

    float CameraProperties::getOrbitalCameraPhi() {
        std::scoped_lock guard(mtxData.first);

        // Make sure we are in the orbital camera mode.
        if (mtxData.second.currentCameraMode == CameraMode::FREE) [[unlikely]] {
            Logger::get().warn(
                "an attempt was made to get orbital camera's phi while the camera is not "
                "in the orbital mode",
                sCameraPropertiesLogCategory);
        }

        return mtxData.second.orbitalModeData.phi;
    }

    float CameraProperties::getNearClipPlaneDistance() {
        std::scoped_lock guard(mtxData.first);

        return mtxData.second.projectionData.second.nearClipPlaneDistance;
    }

    float CameraProperties::getFarClipPlaneDistance() {
        std::scoped_lock guard(mtxData.first);

        return mtxData.second.projectionData.second.farClipPlaneDistance;
    }

    float CameraProperties::getAspectRatio() {
        std::scoped_lock guard(mtxData.first);

        return static_cast<float>(mtxData.second.projectionData.second.iRenderTargetWidth) /
               static_cast<float>(mtxData.second.projectionData.second.iRenderTargetHeight);
    }

    glm::mat4x4 CameraProperties::getViewMatrix() {
        std::scoped_lock guard(mtxData.first);

        makeSureViewMatrixIsUpToDate();

        return mtxData.second.viewMatrix.second;
    }

    glm::mat4x4 CameraProperties::getProjectionMatrix() {
        std::scoped_lock guard(mtxData.first);

        makeSureProjectionMatrixAndClipPlanesAreUpToDate();

        return mtxData.second.projectionData.second.projectionMatrix;
    }

    void CameraProperties::makeSureViewMatrixIsUpToDate() {
        std::scoped_lock guard(mtxData.first);

        // Only continue if the view matrix is marked as "needs update".
        if (!mtxData.second.viewMatrix.first) {
            return;
        }

        // Construct location and up direction considering world adjustment matrix.
        const glm::vec3 location = getLocation(true);
        const glm::vec3 upDirection = getUpDirection(true);

        if (mtxData.second.currentCameraMode == CameraMode::FREE) {
            // Construct forward direction considering world adjustment matrix.
            const glm::vec3 forwardDirection = getForwardDirection(true);

            // Construct view matrix.
            mtxData.second.viewMatrix.second =
                glm::lookAtLH(location, location + forwardDirection, upDirection);
        } else {
            // Construct target location considering world adjustment matrix.
            const glm::vec3 targetLocation = getOrbitalCameraTargetLocation(true);

            // Construct view matrix.
            mtxData.second.viewMatrix.second = glm::lookAtLH(location, targetLocation, upDirection);
        }

        // Mark view matrix as "updated".
        mtxData.second.viewMatrix.first = false;
    }

    void CameraProperties::makeSureProjectionMatrixAndClipPlanesAreUpToDate() {
        std::scoped_lock guard(mtxData.first);

        // Make sure that we actually need to recalculate it.
        if (!mtxData.second.projectionData.first) {
            return;
        }

        const auto verticalFovInRadians =
            glm::radians(static_cast<float>(mtxData.second.projectionData.second.iVerticalFov));

        // Calculate projection matrix.
        mtxData.second.projectionData.second.projectionMatrix = glm::perspectiveFovLH(
            verticalFovInRadians,
            static_cast<float>(mtxData.second.projectionData.second.iRenderTargetWidth),
            static_cast<float>(mtxData.second.projectionData.second.iRenderTargetHeight),
            mtxData.second.projectionData.second.nearClipPlaneDistance,
            mtxData.second.projectionData.second.farClipPlaneDistance);

        // Projection window width/height in normalized device coordinates.
        constexpr float projectionWindowDimensionSize = 2.0F; // because view space window is [-1; 1]

        const auto tan = std::tanf(0.5F * verticalFovInRadians);

        // Calculate clip planes height.
        mtxData.second.projectionData.second.nearClipPlaneHeight =
            projectionWindowDimensionSize * mtxData.second.projectionData.second.nearClipPlaneDistance * tan;
        mtxData.second.projectionData.second.farClipPlaneHeight =
            projectionWindowDimensionSize * mtxData.second.projectionData.second.farClipPlaneDistance * tan;

        // Change flag.
        mtxData.second.projectionData.first = false;
    }

    void CameraProperties::setWorldAdjustmentMatrix(const glm::mat4x4& adjustmentMatrix) {
        std::scoped_lock guard(mtxData.first);

        mtxData.second.worldAdjustmentMatrix = adjustmentMatrix;

        // Mark view matrix as "needs update"
        mtxData.second.viewMatrix.first = true;
    }

} // namespace ne
