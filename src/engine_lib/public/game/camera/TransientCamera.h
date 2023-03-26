#pragma once

// Custom.
#include "game/camera/CameraProperties.h"

namespace ne {
    /**
     * Camera that can be used when there's no world exist (so CameraNode can't be used) or when you
     * don't want to modify world's node tree with your CameraNode (for ex. the engine editor's camera).
     */
    class TransientCamera {
        // Controls movement.
        friend class CameraManager;

    public:
        TransientCamera() = default;

        /**
         * Returns camera properties.
         *
         * @warning Do not delete returned pointer.
         *
         * @return Camera properties.
         */
        CameraProperties* getProperties();

    private:
        /**
         * Makes free camera to constantly move forward/backward.
         *
         * @remark In order to stop the movement specify 0 as input.
         *
         * @remark Logs a warning if the camera is in orbital mode (see @ref getProperties).
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
         * @remark Logs a warning if the camera is in orbital mode (see @ref getProperties).
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
         * @remark Logs a warning if the camera is in orbital mode (see @ref getProperties).
         *
         * @param input User input in range [-1.0F, 1.0F] where 0 means no input and 1 means full input
         * strength.
         */
        void setFreeCameraWorldUpMovement(float input);

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

        /** Camera properties. */
        CameraProperties properties;

        /**
         * Last input specified in @ref setFreeCameraForwardMovement (X) and
         * @ref setFreeCameraRightMovement (Y).
         */
        glm::vec3 lastInputDirection = glm::vec3(0.0F, 0.0F, 0.0F);

        /** Whether @ref lastInputDirection is zero or not. */
        bool bHasInputToProcess = false;

        /** Delta to compare input to zero. */
        static inline constexpr float inputDelta = 0.001F;
    };
} // namespace ne
