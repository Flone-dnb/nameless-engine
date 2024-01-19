#pragma once

// Standard.
#include <memory>
#include <mutex>

// Custom.
#include "misc/GC.hpp"

namespace ne {
    class CameraNode;
    class Renderer;
    class CameraProperties;

    /** Determines what camera is used to draw on the screen. */
    class CameraManager {
        // Active camera node will notify the manager when it's being despawned.
        friend class CameraNode;

    public:
        CameraManager() = delete;

        /**
         * Creates a new manager.
         *
         * @param pRenderer Used renderer.
         */
        CameraManager(Renderer* pRenderer);

        /**
         * Makes a camera node to be the primary camera.
         *
         * @remark Only spawned camera nodes can be used here, otherwise an error will be shown.
         *
         * @remark Previously active camera (if there was one) will become inactive.
         *
         * @param pCameraNode Spawned camera node to make active.
         */
        void setActiveCamera(const gc<CameraNode>& pCameraNode);

        /** Removes the currently active camera so that there will be no active camera. */
        void clearActiveCamera();

        /**
         * Returns the currently active camera.
         *
         * @warning Don't change pointers to cameras in returned object, only copy pointers or modify
         * camera/node properties.
         *
         * @warning Must be used with the mutex.
         *
         * @return `nullptr` if there is no active camera, otherwise valid camera.
         */
        std::pair<std::recursive_mutex, gc<CameraNode>>* getActiveCamera();

    private:
        /**
         * Called by an active camera node when it's being despawned.
         *
         * @param pCameraNode Camera node that's being despawned.
         */
        void onCameraNodeDespawning(CameraNode* pCameraNode);

        /** Used renderer. */
        Renderer* pRenderer = nullptr;

        /** Stores active camera. */
        std::pair<std::recursive_mutex, gc<CameraNode>> mtxActiveCamera;
    };
} // namespace ne
