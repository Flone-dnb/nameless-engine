#include "game/camera/CameraProperties.h"

// Standard.
#include <format>

// Custom.
#include "misc/Error.h"
#include "misc/Profiler.hpp"

namespace ne {

    void CameraProperties::setAspectRatio(unsigned int iRenderTargetWidth, unsigned int iRenderTargetHeight) {
        std::scoped_lock guard(mtxData.first);

        // Make sure this aspect ratio is different.
        if (mtxData.second.projectionData.iRenderTargetWidth == iRenderTargetWidth &&
            mtxData.second.projectionData.iRenderTargetHeight == iRenderTargetHeight) {
            return;
        }

        // Apply change.
        mtxData.second.projectionData.iRenderTargetWidth = iRenderTargetWidth;
        mtxData.second.projectionData.iRenderTargetHeight = iRenderTargetHeight;

        // Mark projection matrix as "needs update".
        mtxData.second.projectionData.bProjectionMatrixNeedsUpdate = true;
    }

    void CameraProperties::setFov(unsigned int iVerticalFov) {
        std::scoped_lock guard(mtxData.first);

        // Apply FOV.
        mtxData.second.projectionData.iVerticalFov = iVerticalFov;

        // Mark projection matrix as "needs update".
        mtxData.second.projectionData.bProjectionMatrixNeedsUpdate = true;
    }

    void CameraProperties::setNearClipPlaneDistance(float nearClipPlaneDistance) {
        if (nearClipPlaneDistance < CameraProperties::Data::minimumClipPlaneDistance) [[unlikely]] {
            Error error(std::format(
                "the specified near clip plane distance {} is lower than the minimum allowed clip plane "
                "distance: {}",
                nearClipPlaneDistance,
                CameraProperties::Data::minimumClipPlaneDistance));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        std::scoped_lock guard(mtxData.first);

        // Apply near clip plane.
        mtxData.second.projectionData.nearClipPlaneDistance = nearClipPlaneDistance;

        // Mark projection matrix as "needs update".
        mtxData.second.projectionData.bProjectionMatrixNeedsUpdate = true;
    }

    void CameraProperties::setFarClipPlaneDistance(float farClipPlaneDistance) {
        if (farClipPlaneDistance < CameraProperties::Data::minimumClipPlaneDistance) [[unlikely]] {
            Error error(std::format(
                "the specified far clip plane distance {} is lower than the minimum allowed clip plane "
                "distance: {}",
                farClipPlaneDistance,
                CameraProperties::Data::minimumClipPlaneDistance));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        std::scoped_lock guard(mtxData.first);

        // Apply far clip plane.
        mtxData.second.projectionData.farClipPlaneDistance = farClipPlaneDistance;

        // Mark projection matrix as "needs update".
        mtxData.second.projectionData.bProjectionMatrixNeedsUpdate = true;
    }

    unsigned int CameraProperties::getVerticalFov() {
        std::scoped_lock guard(mtxData.first);

        return mtxData.second.projectionData.iVerticalFov;
    }

    float CameraProperties::getNearClipPlaneDistance() {
        std::scoped_lock guard(mtxData.first);

        return mtxData.second.projectionData.nearClipPlaneDistance;
    }

    float CameraProperties::getFarClipPlaneDistance() {
        std::scoped_lock guard(mtxData.first);

        return mtxData.second.projectionData.farClipPlaneDistance;
    }

    unsigned int CameraProperties::getRenderTargetWidth() {
        std::scoped_lock guard(mtxData.first);

        return mtxData.second.projectionData.iRenderTargetWidth;
    }

    unsigned int CameraProperties::getRenderTargetHeight() {
        std::scoped_lock guard(mtxData.first);

        return mtxData.second.projectionData.iRenderTargetHeight;
    }

    CameraMode CameraProperties::getCurrentCameraMode() {
        std::scoped_lock guard(mtxData.first);

        return mtxData.second.currentCameraMode;
    }

    CameraProperties::Data::OrbitalModeData CameraProperties::getOrbitalModeProperties() {
        std::scoped_lock guard(mtxData.first);
        return mtxData.second.orbitalModeData;
    }

    glm::vec3 CameraProperties::getWorldLocation() {
        std::scoped_lock guard(mtxData.first);
        return mtxData.second.viewData.worldLocation;
    }

    glm::mat4x4 CameraProperties::getViewMatrix() {
        std::scoped_lock guard(mtxData.first);

        makeSureViewMatrixIsUpToDate();

        return mtxData.second.viewData.viewMatrix;
    }

    glm::mat4x4 CameraProperties::getProjectionMatrix() {
        std::scoped_lock guard(mtxData.first);

        makeSureProjectionMatrixAndClipPlanesAreUpToDate();

        return mtxData.second.projectionData.projectionMatrix;
    }

    void CameraProperties::makeSureViewMatrixIsUpToDate() {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxData.first);

        // Only continue if the view matrix is marked as "needs update".
        if (!mtxData.second.viewData.bViewMatrixNeedsUpdate) {
            return;
        }

        // Calculate view matrix.
        mtxData.second.viewData.viewMatrix = glm::lookAtLH(
            mtxData.second.viewData.worldLocation,
            mtxData.second.viewData.targetPointWorldLocation,
            mtxData.second.viewData.worldUpDirection);

        // Mark view matrix as "updated".
        mtxData.second.viewData.bViewMatrixNeedsUpdate = false;
    }

    void CameraProperties::makeSureProjectionMatrixAndClipPlanesAreUpToDate() {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxData.first);

        // Make sure that we actually need to recalculate it.
        if (!mtxData.second.projectionData.bProjectionMatrixNeedsUpdate) {
            return;
        }

        const auto verticalFovInRadians =
            glm::radians(static_cast<float>(mtxData.second.projectionData.iVerticalFov));

        // Calculate projection matrix.
        mtxData.second.projectionData.projectionMatrix = glm::perspectiveFovLH(
            verticalFovInRadians,
            static_cast<float>(mtxData.second.projectionData.iRenderTargetWidth),
            static_cast<float>(mtxData.second.projectionData.iRenderTargetHeight),
            mtxData.second.projectionData.nearClipPlaneDistance,
            mtxData.second.projectionData.farClipPlaneDistance);

        // Projection window width/height in normalized device coordinates.
        constexpr float projectionWindowDimensionSize = 2.0F; // because view space window is [-1; 1]

        const auto tan = std::tanf(0.5F * verticalFovInRadians);

        // Calculate clip planes height.
        mtxData.second.projectionData.nearClipPlaneHeight =
            projectionWindowDimensionSize * mtxData.second.projectionData.nearClipPlaneDistance * tan;
        mtxData.second.projectionData.farClipPlaneHeight =
            projectionWindowDimensionSize * mtxData.second.projectionData.farClipPlaneDistance * tan;

        // Change flag.
        mtxData.second.projectionData.bProjectionMatrixNeedsUpdate = false;
    }

} // namespace ne
