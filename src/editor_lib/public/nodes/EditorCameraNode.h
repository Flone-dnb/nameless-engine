#pragma once

// Custom.
#include "game/nodes/CameraNode.h"

namespace ne {
    /** Camera used in the editor. */
    class EditorCameraNode : public CameraNode {
    public:
        EditorCameraNode();

        /**
         * Creates a new node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        EditorCameraNode(const std::string& sNodeName);

        virtual ~EditorCameraNode() override = default;

        /**
         * Sets whether to ignore user input or not.
         *
         * @param bIgnore `true` to ignore, `false` otherwise.
         */
        void setIgnoreInput(bool bIgnore);

    protected:
        /**
         * Called before a new frame is rendered.
         *
         * @remark This function will only be called while this node is spawned.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic (if there is any).
         *
         * @param timeSincePrevFrameInSec Also known as deltatime - time in seconds that has passed since
         * the last frame was rendered.
         */
        virtual void onBeforeNewFrame(float timeSincePrevFrameInSec) override;

        /**
         * Called when the window received mouse movement.
         *
         * @remark This function will only be called while this node is spawned.
         *
         * @param xOffset  Mouse X movement delta in pixels (plus if moved to the right,
         * minus if moved to the left).
         * @param yOffset  Mouse Y movement delta in pixels (plus if moved up,
         * minus if moved down).
         */
        virtual void onMouseMove(double xOffset, double yOffset) override;

        /**
         * Called after this node or one of the node's parents (in the parent hierarchy)
         * was attached to a new parent node.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         *
         * @remark This function will also be called on all child nodes after this function
         * is finished.
         *
         * @param bThisNodeBeingAttached `true` if this node was attached to a parent,
         * `false` if some node in the parent hierarchy was attached to a parent.
         */
        virtual void onAfterAttachedToNewParent(bool bThisNodeBeingAttached) override;

    private:
        /** Last received user input direction for moving the camera. */
        glm::vec3 lastInputDirection = glm::vec3(0.0F, 0.0F, 0.0F);

        /** Editor camera's current movement speed. */
        float currentMovementSpeed = 0.0F;

        /**
         * Stores @ref speedIncreaseMultiplier or @ref speedDecreaseMultiplier when the user holds
         * a special button.
         */
        float currentMovementSpeedMultiplier = 1.0F;

        /**
         * Determines whether the camera should ignore the user input or not.
         * Generally we only want to process user input when some special condition is met (for ex. mouse
         * captured).
         */
        bool bIgnoreInput = true;

        /** Rotation multiplier for editor's camera. */
        static constexpr double rotationSensitivity = 0.1; // NOLINT: default value

        /** Speed of editor camera's movement. */
        static constexpr float movementSpeed = 5.0F;

        /** Camera speed multiplier when fast movement mode is enabled (for ex. Shift is pressed). */
        static constexpr float speedIncreaseMultiplier = 2.0F;

        /** Camera speed multiplier when slow movement mode is enabled (for ex. Ctrl is pressed). */
        static constexpr float speedDecreaseMultiplier = 0.5F;
    };
}
