#include "game/camera/CameraManager.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"
#include "game/nodes/CameraNode.h"
#include "misc/Profiler.hpp"
#include "render/Renderer.h"

namespace ne {

    CameraManager::CameraManager(Renderer* pRenderer) { this->pRenderer = pRenderer; }

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
            Error error(std::format(
                "camera node \"{}\" needs to be spawned in order to make it the active camera",
                pCameraNode->getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // don't unlock the node mutex yet

        if (mtxActiveCamera.second != nullptr) {
            // Mark previous as inactive.
            std::scoped_lock cameraGuard(mtxActiveCamera.second->mtxIsActive.first);
            mtxActiveCamera.second->mtxIsActive.second = false;
        }

        {
            // Mark new camera node as active.
            std::scoped_lock cameraGuard(pCameraNode->mtxIsActive.first);
            pCameraNode->mtxIsActive.second = true;
        }

        mtxActiveCamera.second = pCameraNode;

        // Notify renderer.
        pRenderer->onActiveCameraChanged();
    }

    void CameraManager::clearActiveCamera() {
        std::scoped_lock guard(mtxActiveCamera.first);

        if (mtxActiveCamera.second != nullptr) {
            // Mark previous as inactive.
            std::scoped_lock cameraGuard(mtxActiveCamera.second->mtxIsActive.first);
            mtxActiveCamera.second->mtxIsActive.second = false;
        }

        // Clear pointer.
        mtxActiveCamera.second = nullptr;

        // Notify renderer.
        pRenderer->onActiveCameraChanged();
    }

    std::pair<std::recursive_mutex, gc<CameraNode>>* CameraManager::getActiveCamera() {
        return &mtxActiveCamera;
    }

    void CameraManager::onCameraNodeDespawning(CameraNode* pCameraNode) {
        std::scoped_lock guard(mtxActiveCamera.first);

        // Make sure there's an active camera.
        if (mtxActiveCamera.second == nullptr) [[unlikely]] {
            Logger::get().error(std::format(
                "the camera node \"{}\" notified the camera manager about it being despawned because "
                "it thinks that it's the active camera but the camera manager has no active camera node",
                pCameraNode->getNodeName()));
            return;
        }

        // See if this camera is indeed used as the active one.
        if (&*mtxActiveCamera.second != pCameraNode) [[unlikely]] {
            Logger::get().error(std::format(
                "the camera node \"{}\" notified the camera manager about it being despawned because "
                "it thinks that it's the active camera but it's not the active camera node",
                pCameraNode->getNodeName()));
            return;
        }

        {
            // Mark the camera as inactive.
            std::scoped_lock cameraGuard(mtxActiveCamera.second->mtxIsActive.first);
            mtxActiveCamera.second->mtxIsActive.second = false;
        }

        // No active camera now.
        mtxActiveCamera.second = nullptr;
    }

} // namespace ne
