#pragma once

// Custom.
#include "game/nodes/SpatialNode.h"
#include "game/camera/CameraProperties.h"

#include "CameraNode.generated.h"

namespace ne RNAMESPACE() {
    /** Represents a camera in 3D space. */
    class RCLASS(Guid("d0fdb87f-099e-479a-8975-d9db1c40488e")) CameraNode : public SpatialNode {
    public:
        CameraNode();

        /**
         * Creates a new node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        CameraNode(const std::string& sNodeName);

        virtual ~CameraNode() override = default;

        /**
         * Returns camera properties.
         *
         * @warning Do not delete returned pointer.
         *
         * @return Camera properties.
         */
        CameraProperties* getCameraProperties();

    protected:
        /**
         * Called after node's world location/rotation/scale was changed.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onWorldLocationRotationScaleChanged() override;

    private:
        /** Camera properties. */
        CameraProperties cameraProperties;

        ne_CameraNode_GENERATED
    };
} // namespace ne RNAMESPACE()

File_CameraNode_GENERATED
