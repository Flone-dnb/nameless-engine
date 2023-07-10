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
         * Sets how the camera can move and rotate.
         *
         * @param mode New mode.
         */
        void setCameraMode(CameraMode mode);

        /**
         * Sets a location in world space that orbital camera should look at
         * (when not set, orbital camera looks at the node's local space origin,
         * see @ref clearOrbitalTargetLocation).
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param targetPointLocation Location in world space to look at.
         */
        void setOrbitalTargetLocation(const glm::vec3& targetPointLocation);

        /**
         * Resets target point specified in @ref setOrbitalTargetLocation so that orbital camera
         * will look at node's local space origin.
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         */
        void clearOrbitalTargetLocation();

        /**
         * Sets orbital camera's rotation by specifying tilt and rotation around camera's target point
         * (see @ref setOrbitalTargetLocation).
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param phi   Azimuthal angle (in degrees).
         * @param theta Polar angle (in degrees).
         */
        void setOrbitalRotation(float phi, float theta);

        /**
         * Sets orbital camera's radial distance or distance from camera to camera's target point
         * (see @ref setOrbitalTargetLocation).
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param distanceToTarget Radial distance or distance from camera to camera's target point.
         */
        void setOrbitalDistanceToTarget(float distanceToTarget);

        /**
         * Returns location in world space to where the orbital camera looks at
         * (also see @ref setOrbitalTargetLocation).
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @return Location in world space.
         */
        glm::vec3 getOrbitalTargetLocation();

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
        /** Applies current location/rotation to camera properties based on the current camera mode. */
        void updateCameraProperties();

        /** Camera properties. */
        CameraProperties cameraProperties;

        /** If not empty used instead of @ref localSpaceOriginInWorldSpace. */
        std::optional<glm::vec3> orbitalCameraTargetInWorldSpace = {};

        /** (0.0F, 0.0F, 0.0F) in local space converted to world space. */
        glm::vec3 localSpaceOriginInWorldSpace = glm::vec3(0.0F, 0.0F, 0.0F);

        /**
         * Whether this camera is used by the camera manager or not.
         *
         * @warning Only camera manager can change this value.
         */
        std::pair<std::recursive_mutex, bool> mtxIsActive;

        /** Delta to compare rotations. */
        static inline constexpr float rotationDelta = 0.0001F;

        ne_CameraNode_GENERATED
    };
} // namespace ne RNAMESPACE()

File_CameraNode_GENERATED
