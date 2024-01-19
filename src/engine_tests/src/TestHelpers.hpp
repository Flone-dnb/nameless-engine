#pragma once

// Custom.
#include "game/nodes/CameraNode.h"
#include "game/camera/CameraManager.h"

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
        static inline gc<CameraNode>
        createAndSpawnActiveCamera(const gc<Node>& pWorldRootNode, CameraManager* pCameraManager) {
            // Create camera.
            auto pCamera = gc_new<CameraNode>();

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
