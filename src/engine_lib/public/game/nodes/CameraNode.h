#pragma once

// Custom.
#include "game/nodes/SpatialNode.h"
#include "game/camera/CameraProperties.h"

#include "CameraNode.generated.h"

namespace ne RNAMESPACE() {
    /** Represents a camera in 3D space. */
    class RCLASS(Guid("d0fdb87f-099e-479a-8975-d9db1c40488e")) CameraNode : public SpatialNode {
        // Tells whether this node is being active camera or not.
        friend class CameraManager;

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

        /**
         * Called before this node is despawned from the world to execute custom despawn logic.
         *
         * @remark This node will be marked as despawned after this function is called.
         * @remark This function is called after all child nodes were despawned.
         * @remark If node's destructor is called but node is still spawned it will be despawned.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onDespawning() override;

    private:
        /** Camera properties. */
        CameraProperties cameraProperties;

        /**
         * Whether this camera is used by the camera manager or not.
         *
         * @warning Only camera manager can change this value.
         */
        std::pair<std::recursive_mutex, bool> mtxIsActive;

        ne_CameraNode_GENERATED
    };
} // namespace ne RNAMESPACE()

File_CameraNode_GENERATED
