#include "game/camera/CameraManager.h"

// Custom.
#include "io/Logger.h"
#include "game/nodes/CameraNode.h"
#include "game/camera/TransientCamera.h"
#include "misc/Profiler.hpp"

namespace ne {

    void CameraManager::setActiveCamera(std::shared_ptr<TransientCamera> pTransientCamera) {
        if (pTransientCamera == nullptr) [[unlikely]] {
            Error error("`nullptr` is not a valid camera");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        std::scoped_lock guard(mtxActiveCamera.first);

        markPreviousCameraAsInactive();

        mtxActiveCamera.second.pCameraNode = nullptr;
        mtxActiveCamera.second.pTransientCamera = std::move(pTransientCamera);
    }

    void CameraManager::setActiveCamera(const gc<CameraNode>& pCameraNode) {
        if (pCameraNode == nullptr) [[unlikely]] {
            Error error("`nullptr` is not a valid camera");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        std::scoped_lock guard(mtxActiveCamera.first);

        // Make sure this node is spawned.
        std::scoped_lock nodeSpawnGuard(*pCameraNode->getSpawnDespawnMutex());
        if (!pCameraNode->isSpawned()) [[unlikely]] {
            Error error(fmt::format(
                "camera node \"{}\" needs to be spawned in order to make it the active camera",
                pCameraNode->getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // don't unlock the node mutex yet

        markPreviousCameraAsInactive();

        {
            // Mark new camera node as active.
            std::scoped_lock cameraGuard(pCameraNode->mtxIsActive.first);
            pCameraNode->mtxIsActive.second = true;
        }

        mtxActiveCamera.second.pCameraNode = pCameraNode;
        mtxActiveCamera.second.pTransientCamera = nullptr;
    }

    void CameraManager::clearActiveCamera() {
        std::scoped_lock guard(mtxActiveCamera.first);

        markPreviousCameraAsInactive();

        mtxActiveCamera.second.pCameraNode = nullptr;
        mtxActiveCamera.second.pTransientCamera = nullptr;
    }

    std::pair<std::recursive_mutex, CameraManager::ActiveCamera>* CameraManager::getActiveCamera() {
        return &mtxActiveCamera;
    }

    void CameraManager::onCameraNodeDespawning(CameraNode* pCameraNode) {
        std::scoped_lock guard(mtxActiveCamera.first);

        // Make sure there's an active camera.
        if (mtxActiveCamera.second.pCameraNode == nullptr) [[unlikely]] {
            Logger::get().error(fmt::format(
                "the camera node \"{}\" notified the camera manager about it being despawned because "
                "it thinks that it's the active camera but the camera manager has no active camera node",
                pCameraNode->getNodeName()));
            return;
        }

        // See if this camera is used as the active one.
        if (&*mtxActiveCamera.second.pCameraNode != pCameraNode) [[unlikely]] {
            Logger::get().error(fmt::format(
                "the camera node \"{}\" notified the camera manager about it being despawned because "
                "it thinks that it's the active camera but it's not the active camera node",
                pCameraNode->getNodeName()));
            return;
        }

        // Mark the camera as inactive.
        std::scoped_lock cameraGuard(mtxActiveCamera.second.pCameraNode->mtxIsActive.first);
        mtxActiveCamera.second.pCameraNode->mtxIsActive.second = false;

        // No active camera now.
        mtxActiveCamera.second.pCameraNode = nullptr;
    }

    void CameraManager::onBeforeNewFrame(float timeSincePrevCallInSec) {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxActiveCamera.first);

        // Call on the currently active transient camera.
        if (mtxActiveCamera.second.pTransientCamera != nullptr) {
            mtxActiveCamera.second.pTransientCamera->onBeforeNewFrame(timeSincePrevCallInSec);
        }
    }

    void CameraManager::markPreviousCameraAsInactive() {
        std::scoped_lock guard(mtxActiveCamera.first);

        if (mtxActiveCamera.second.pCameraNode != nullptr) {
            // Mark as inactive.
            std::scoped_lock cameraGuard(mtxActiveCamera.second.pCameraNode->mtxIsActive.first);
            mtxActiveCamera.second.pCameraNode->mtxIsActive.second = false;
        } else if (mtxActiveCamera.second.pTransientCamera != nullptr) {
            // Clear any saved input.
            mtxActiveCamera.second.pTransientCamera->clearInput();
        }
    }

} // namespace ne
