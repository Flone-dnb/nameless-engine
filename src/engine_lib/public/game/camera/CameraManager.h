#pragma once

// Standard.
#include <memory>
#include <mutex>

// Custom.
#include "game/camera/TransientCamera.h"
#include "misc/GC.hpp"

namespace ne {
    class CameraNode;

    /** Determines what camera is used to draw on the screen. */
    class CameraManager {
        // Active camera node will notify the manager when it's being despawned.
        friend class CameraNode;

        // Calls "on before new frame".
        friend class Game;

    public:
        /** Stores active camera. It's either a transient camera or a camera node (never both). */
        struct ActiveCamera {
            /** Transient camera. */
            std::unique_ptr<TransientCamera> pTransientCamera;

            /** Camera node spawned in world. */
            gc<CameraNode> pCameraNode;
        };

        CameraManager() = default;

        /**
         * Takes ownership of a transient camera an makes it the primary camera.
         *
         * @param pTransientCamera Camera to make active.
         */
        void setActiveCamera(std::unique_ptr<TransientCamera> pTransientCamera);

        /**
         * Makes a camera node to be the primary camera.
         *
         * @remark Only spawned camera nodes can be used here, otherwise an error will be shown.
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

        /** Stores active camera. */
        std::pair<std::recursive_mutex, ActiveCamera> mtxActiveCamera;

        /** Name of the category used for logging. */
        static inline const auto sCameraManagerLogCategory = "Camera Manager";
    };
} // namespace ne
