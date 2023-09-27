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

        auto& viewData = mtxData.second.viewData;

        // Calculate view matrix.
        viewData.viewMatrix = glm::lookAtLH(
            viewData.worldLocation, viewData.targetPointWorldLocation, viewData.worldUpDirection);

        // Since we finished updating view data - recalculate frustum.
        recalculateFrustum();

        // Mark view data as updated.
        mtxData.second.viewData.bViewMatrixNeedsUpdate = false;
    }

    void CameraProperties::makeSureProjectionMatrixAndClipPlanesAreUpToDate() {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxData.first);

        auto& projectionData = mtxData.second.projectionData;

        // Make sure that we actually need to recalculate it.
        if (!projectionData.bProjectionMatrixNeedsUpdate) {
            return;
        }

        const auto verticalFovInRadians = glm::radians(static_cast<float>(projectionData.iVerticalFov));

        // Calculate projection matrix.
        projectionData.projectionMatrix = glm::perspectiveFovLH(
            verticalFovInRadians,
            static_cast<float>(projectionData.iRenderTargetWidth),
            static_cast<float>(projectionData.iRenderTargetHeight),
            projectionData.nearClipPlaneDistance,
            projectionData.farClipPlaneDistance);

        // Projection window width/height in normalized device coordinates.
        constexpr float projectionWindowDimensionSize = 2.0F; // because view space window is [-1; 1]

        const auto tanHalfFov = std::tan(0.5F * verticalFovInRadians);

        // Calculate clip planes height.
        projectionData.nearClipPlaneHeight =
            projectionWindowDimensionSize * projectionData.nearClipPlaneDistance * tanHalfFov;
        projectionData.farClipPlaneHeight =
            projectionWindowDimensionSize * projectionData.farClipPlaneDistance * tanHalfFov;

        // Since we finished updating projection data - recalculate frustum.
        recalculateFrustum();

        // Mark projection data as updated.
        projectionData.bProjectionMatrixNeedsUpdate = false;
    }

    void CameraProperties::recalculateFrustum() {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxData.first);

        // Prepare some variables.
        auto& viewData = mtxData.second.viewData;
        auto& projectionData = mtxData.second.projectionData;
        auto& frustum = mtxData.second.frustum;
        const auto verticalFovInRadians = glm::radians(static_cast<float>(projectionData.iVerticalFov));

        // Precalculate `tan(fov/2)` because we will need it multiple times.
        // By using the following rule: tan(X) = opposite side / adjacent side
        // this value gives us: far clip plane half height / z far
        //                  /|
        //                 / |
        //                /  |  <- camera frustum from side view (not top view)
        //               /   |
        // camera:   fov ----- zFar
        //               \   |
        //                \  |  <- frustum half height
        //                 \ |
        //                  \|
        const auto tanHalfFov = std::tan(0.5F * verticalFovInRadians);
        const auto farClipPlaneHalfHeight = projectionData.farClipPlaneDistance * tanHalfFov;
        const auto farClipPlaneHalfWidth =
            farClipPlaneHalfHeight * (static_cast<float>(projectionData.iRenderTargetWidth) /
                                      static_cast<float>(projectionData.iRenderTargetHeight));

        // Prepare some camera directions in world space to recalculate frustum.
        const auto cameraWorldForward = glm::normalize(
            mtxData.second.viewData.targetPointWorldLocation - mtxData.second.viewData.worldLocation);
        const auto cameraWorldRight =
            glm::normalize(glm::cross(mtxData.second.viewData.worldUpDirection, cameraWorldForward));
        const auto toFarPlaneRelativeCameraLocation =
            projectionData.farClipPlaneDistance * cameraWorldForward;

        // Update frustum near face.
        frustum.nearFace = Plane(
            cameraWorldForward,
            viewData.worldLocation + projectionData.nearClipPlaneDistance * cameraWorldForward);

        // Update frustum far face.
        frustum.farFace =
            Plane(-cameraWorldForward, viewData.worldLocation + toFarPlaneRelativeCameraLocation);

        // Update frustum right face.
        frustum.rightFace = Plane(
            glm::normalize(glm::cross(
                toFarPlaneRelativeCameraLocation + cameraWorldRight * farClipPlaneHalfWidth,
                viewData.worldUpDirection)),
            viewData.worldLocation);

        // Update frustum left face.
        frustum.leftFace = Plane(
            glm::normalize(glm::cross(
                viewData.worldUpDirection,
                toFarPlaneRelativeCameraLocation - cameraWorldRight * farClipPlaneHalfWidth)),
            viewData.worldLocation);

        // Update frustum top face.
        frustum.topFace = Plane(
            glm::normalize(glm::cross(
                cameraWorldRight,
                toFarPlaneRelativeCameraLocation + viewData.worldUpDirection * farClipPlaneHalfHeight)),
            viewData.worldLocation);

        // Update frustum bottom face.
        frustum.bottomFace = Plane(
            glm::normalize(glm::cross(
                toFarPlaneRelativeCameraLocation - viewData.worldUpDirection * farClipPlaneHalfHeight,
                cameraWorldRight)),
            viewData.worldLocation);
    }

} // namespace ne
