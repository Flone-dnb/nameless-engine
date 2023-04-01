#include "game/nodes/CameraNode.h"

// Custom.
#include "game/GameInstance.h"
#include "game/camera/CameraManager.h"

#include "CameraNode.generated_impl.h"

namespace ne {

    CameraNode::CameraNode() : CameraNode("Camera Node") {}

    CameraNode::CameraNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
        mtxIsActive.second = false;
    }

    void CameraNode::onWorldLocationRotationScaleChanged() {
        SpatialNode::onWorldLocationRotationScaleChanged();

        // Set world matrix to be included in camera's view matrix.
        cameraProperties.setWorldAdjustmentMatrix(getWorldMatrix());
    }

    CameraProperties* CameraNode::getCameraProperties() { return &cameraProperties; }

    void CameraNode::onDespawning() {
        SpatialNode::onDespawning();

        {
            std::scoped_lock guardActive(mtxIsActive.first);
            if (mtxIsActive.second) {
                // Notify camera manager.
                getGameInstance()->getCameraManager()->onCameraNodeDespawning(this);
            }
        }
    }

} // namespace ne
