#pragma once

// Standard.
#include <memory>
#include <mutex>

// Custom.
#include "misc/GC.hpp"

namespace ne {
    class CameraNode;
    class TransientCamera;
    class Renderer;
    class CameraProperties;

    /** Determines what camera is used to draw on the screen. */
    class CameraManager {
        // Active camera node will notify the manager when it's being despawned.
        friend class CameraNode;

        // Calls "on before new frame".
        friend class GameManager;

    public:
        /** Stores active camera. It's either a transient camera or a camera node (never both). */
        struct ActiveCamera {
            /**
             * Returns properties of this camera.
             *
             * @return `nullptr` if no active camera, otherwise active camera properties.
             */
            CameraProperties* getCameraProperties();

            /** Transient camera. */
            std::shared_ptr<TransientCamera> pTransientCamera;

            /** Camera node spawned in world. */
            gc<CameraNode> pCameraNode;
        };

        CameraManager() = delete;

        /**
         * Creates a new manager.
         *
         * @param pRenderer Used renderer.
         */
        CameraManager(Renderer* pRenderer);

        /**
         * Takes a transient camera an makes it the primary camera.
         *
         * @remark Previously active camera (if there was one) will become inactive.
         *
         * @param pTransientCamera Camera to make active.
         */
        void setActiveCamera(std::shared_ptr<TransientCamera> pTransientCamera);

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
         * @return Active camera (`nullptr` in both camera pointers if there is no active camera).
         */
        std::pair<std::recursive_mutex, ActiveCamera>* getActiveCamera();

    private:
        /**
         * Called by an active camera node when it's being despawned.
         *
         * @param pCameraNode Camera node that's being despawned.
         */
        void onCameraNodeDespawning(CameraNode* pCameraNode);

        /**
         * Called before a new frame is rendered.
         *
         * @param timeSincePrevCallInSec Time in seconds that has passed since the last call
         * to this function.
         */
        void onBeforeNewFrame(float timeSincePrevCallInSec);

        /** Marks the currently active camera in @ref mtxActiveCamera as inactive. */
        void markPreviousCameraAsInactive();

        /** Used renderer. */
        Renderer* pRenderer = nullptr;

        /** Stores active camera. */
        std::pair<std::recursive_mutex, ActiveCamera> mtxActiveCamera;
    };
} // namespace ne
