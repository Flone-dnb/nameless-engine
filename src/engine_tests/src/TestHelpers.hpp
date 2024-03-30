#pragma once

// Custom.
#include "game/nodes/CameraNode.h"
#include "game/camera/CameraManager.h"

// External.
#include "GcPtr.h"

namespace ne {
    /** Provides static helper functions for tests. */
    class TestHelpers {
    public:
        TestHelpers() = delete;

        /**
         * Creates a new camera node, configures it to don't affect the game world and makes it active.
         *
         * @param pWorldRootNode
         */
        static inline sgc::GcPtr<CameraNode>
        createAndSpawnActiveCamera(const sgc::GcPtr<Node>& pWorldRootNode, CameraManager* pCameraManager) {
            // Create camera.
            auto pCamera = sgc::makeGc<CameraNode>();

            // Configure.
            pCamera->setSerialize(false);

            // Spawn.
            pWorldRootNode->addChildNode(pCamera);

            // Make active.
            pCameraManager->setActiveCamera(pCamera);

            return pCamera;
        }
    };
}
