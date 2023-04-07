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

    void CameraProperties::setAspectRatio(unsigned int iRenderTargetWidth, unsigned int iRenderTargetHeight) {
        std::scoped_lock guard(mtxData.first);

        // Make sure this aspect ratio is different.
        if (mtxData.second.projectionData.second.iRenderTargetWidth == iRenderTargetWidth &&
            mtxData.second.projectionData.second.iRenderTargetHeight == iRenderTargetHeight) {
            return;
        }

        // Apply change.
        mtxData.second.projectionData.second.iRenderTargetWidth = iRenderTargetWidth;
        mtxData.second.projectionData.second.iRenderTargetHeight = iRenderTargetHeight;

        // Mark projection matrix as "needs update".
        mtxData.second.projectionData.first = true;
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

    unsigned int CameraProperties::getVerticalFov() {
        std::scoped_lock guard(mtxData.first);

        return mtxData.second.projectionData.second.iVerticalFov;
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

    CameraProperties::Data::OrbitalModeData CameraProperties::getOrbitalModeProperties() {
        std::scoped_lock guard(mtxData.first);
        return mtxData.second.orbitalModeData;
    }

    glm::vec3 CameraProperties::getWorldLocation() {
        std::scoped_lock guard(mtxData.first);
        return mtxData.second.viewData.second.worldLocation;
    }

    glm::mat4x4 CameraProperties::getViewMatrix() {
        std::scoped_lock guard(mtxData.first);

        makeSureViewMatrixIsUpToDate();

        return mtxData.second.viewData.second.viewMatrix;
    }

    glm::mat4x4 CameraProperties::getProjectionMatrix() {
        std::scoped_lock guard(mtxData.first);

        makeSureProjectionMatrixAndClipPlanesAreUpToDate();

        return mtxData.second.projectionData.second.projectionMatrix;
    }

    void CameraProperties::makeSureViewMatrixIsUpToDate() {
        std::scoped_lock guard(mtxData.first);

        // Only continue if the view matrix is marked as "needs update".
        if (!mtxData.second.viewData.first) {
            return;
        }

        // Calculate view matrix.
        mtxData.second.viewData.second.viewMatrix = glm::lookAtLH(
            mtxData.second.viewData.second.worldLocation,
            mtxData.second.viewData.second.targetPointWorldLocation,
            mtxData.second.viewData.second.worldUpDirection);

        // Mark view matrix as "updated".
        mtxData.second.viewData.first = false;
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

} // namespace ne
