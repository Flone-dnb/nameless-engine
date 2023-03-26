#include "game/nodes/CameraNode.h"

#include "CameraNode.generated_impl.h"

namespace ne {

    CameraNode::CameraNode() : CameraNode("Camera Node") {}

    CameraNode::CameraNode(const std::string& sNodeName) : SpatialNode(sNodeName) {}

    void CameraNode::onWorldLocationRotationScaleChanged() {
        SpatialNode::onWorldLocationRotationScaleChanged();

        // Set world matrix to be included in camera's view matrix.
        cameraProperties.setWorldAdjustmentMatrix(getWorldMatrix());
    }

    CameraProperties* CameraNode::getCameraProperties() { return &cameraProperties; }

} // namespace ne
