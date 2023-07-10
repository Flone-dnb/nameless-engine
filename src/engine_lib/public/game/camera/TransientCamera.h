#pragma once

// Custom.
#include "game/camera/CameraProperties.h"
#include "math/MathHelpers.hpp"

namespace ne {
    /**
     * Camera that can be used when there's no world exist (so CameraNode can't be used) or when you
     * don't want to modify world's node tree with your CameraNode (for ex. the engine editor's camera).
     */
    class TransientCamera {
        // Calls `onBeforeNewFrame` and `clearInput`.
        friend class CameraManager;

    public:
        TransientCamera() = default;

        /**
         * Sets how the camera can move and rotate.
         *
         * @param mode New mode.
         */
        void setCameraMode(CameraMode mode);

        /**
         * Makes free camera to constantly move forward/backward.
         *
         * @remark In order to stop the movement specify 0 as input.
         *
         * @remark Logs a warning if the camera is in orbital mode (see @ref setCameraMode).
         *
         * @param input User input in range [-1.0F, 1.0F] where 0 means no input and 1 means full input
         * strength.
         */
        void setFreeCameraForwardMovement(float input);

        /**
         * Makes free camera to constantly move right/left.
         *
         * @remark In order to stop the movement specify 0 as input.
         *
         * @remark Logs a warning if the camera is in orbital mode (see @ref setCameraMode).
         *
         * @param input User input in range [-1.0F, 1.0F] where 0 means no input and 1 means full input
         * strength.
         */
        void setFreeCameraRightMovement(float input);

        /**
         * Makes free camera to constantly move up/down according to world's up direction.
         *
         * @remark In order to stop the movement specify 0 as input.
         *
         * @remark Logs a warning if the camera is in orbital mode (see @ref setCameraMode).
         *
         * @param input User input in range [-1.0F, 1.0F] where 0 means no input and 1 means full input
         * strength.
         */
        void setFreeCameraWorldUpMovement(float input);

        /**
         * Sets camera's world location.
         *
         * @param location Location to set.
         */
        void setLocation(const glm::vec3& location);

        /**
         * Sets camera's rotation in world space.
         *
         * @remark Logs a warning if the camera is in orbital mode (see @ref setCameraMode).
         *
         * @param rotation Camera's rotation in degrees (where X is roll, Y is pitch and Z is yaw).
         */
        void setFreeCameraRotation(const glm::vec3& rotation);

        /**
         * Sets orbital camera's target location in world space.
         *
         * @remark Logs a warning if the camera is in free mode (see @ref setCameraMode).
         *
         * @param targetLocation Location for camera to look at.
         */
        void setOrbitalCameraTargetLocation(const glm::vec3& targetLocation);

        /**
         * Sets camera's radial distance or distance from camera to camera's target point
         * (see @ref setOrbitalCameraTargetLocation).
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param distanceToTarget Radial distance or distance from camera to camera's target point.
         */
        void setOrbitalCameraDistanceToTarget(float distanceToTarget);

        /**
         * Sets camera's rotation by specifying tilt and rotation around camera's target point
         * (see @ref setOrbitalCameraTargetLocation).
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param phi   Azimuthal angle (in degrees).
         * @param theta Polar angle (in degrees).
         */
        void setOrbitalCameraRotation(float phi, float theta);

        /**
         * Sets multiplier for camera's movement.
         *
         * @param speed Multiplier.
         */
        void setCameraMovementSpeed(float speed);

        /**
         * Returns camera's rotation in world space.
         *
         * @return Rotation where X is roll, Y is pitch and Z is yaw.
         */
        glm::vec3 getFreeCameraRotation() const;

        /**
         * Returns camera properties.
         *
         * @warning Do not delete returned pointer.
         *
         * @return Camera properties.
         */
        CameraProperties* getCameraProperties();

    private:
        /**
         * Called by camera manager before a new frame is rendered to process movement input.
         *
         * @param timeSincePrevCallInSec Time in seconds that has passed since the last call
         * to this function.
         */
        void onBeforeNewFrame(float timeSincePrevCallInSec);

        /**
         * Called by camera manager after this camera is no longer used to clear any saved input
         * so that the camera will no longer move if some input direction was previously set.
         */
        void clearInput();

        /**
         * Recalculates camera's forward/right/up directions based on orbital camera's location and
         * camera's target point location.
         */
        inline void recalculateBaseVectorsForOrbitalCamera() {
            cameraForwardDirection = MathHelpers::normalizeSafely(
                cameraProperties.mtxData.second.viewData.targetPointWorldLocation -
                cameraProperties.mtxData.second.viewData.worldLocation);
            cameraRightDirection =
                glm::normalize(glm::cross(Globals::WorldDirection::up, cameraForwardDirection));
            cameraUpDirection = glm::cross(cameraForwardDirection, cameraRightDirection);
        }

        /**
         * Moves free camera in the specified direction.
         *
         * @remark Only works if the current camera mode is orbital (see @ref setCameraMode), otherwise
         * logs a warning.
         *
         * @param distance Distance (where X is forward, Y is right and Z is up).
         */
        void moveFreeCamera(const glm::vec3& distance);

        /** Camera properties. */
        CameraProperties cameraProperties;

        /** Camera's forward direction in world space. */
        glm::vec3 cameraForwardDirection = Globals::WorldDirection::forward;

        /** Camera's right direction in world space. */
        glm::vec3 cameraRightDirection = Globals::WorldDirection::right;

        /** Camera's up direction in world space. */
        glm::vec3 cameraUpDirection = Globals::WorldDirection::up;

        /** Camera's world rotation in degrees (roll, pitch, yaw). */
        glm::vec3 cameraRotation = glm::vec3(0.0F, 0.0F, 0.0F);

        /**
         * Last input specified in
         * @ref setFreeCameraForwardMovement (X),
         * @ref setFreeCameraRightMovement (Y) and
         * @ref setFreeCameraWorldUpMovement (Z).
         */
        glm::vec3 lastInputDirection = glm::vec3(0.0F, 0.0F, 0.0F);

        /** Multiplier for movement. */
        float cameraMovementSpeed = 1.0F;

        /** Delta to compare input to zero. */
        static inline constexpr float inputDelta = 0.0001F;
    };
} // namespace ne
