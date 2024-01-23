#include "nodes/EditorCameraNode.h"

// Custom.
#include "input/EditorInputEventIds.hpp"

namespace ne {

    EditorCameraNode::EditorCameraNode() : EditorCameraNode("Editor Camera Node") {}

    EditorCameraNode::EditorCameraNode(const std::string& sNodeName) : CameraNode(sNodeName) {
        // Enable tick.
        setIsCalledEveryFrame(true);

        // Initialize current speed.
        currentMovementSpeed = movementSpeed;

        // Bind axis events.
        {
            // Get axis events.
            const auto pAxisEvents = getAxisEventBindings();
            std::scoped_lock guard(pAxisEvents->first);

            // Bind move right.
            pAxisEvents->second[static_cast<unsigned int>(EditorInputEventIds::Axis::MOVE_CAMERA_FORWARD)] =
                [this](KeyboardModifiers modifiers, float input) { lastInputDirection.x = input; };

            // Bind move forward.
            pAxisEvents->second[static_cast<unsigned int>(EditorInputEventIds::Axis::MOVE_CAMERA_RIGHT)] =
                [this](KeyboardModifiers modifiers, float input) { lastInputDirection.y = input; };

            // Bind move up.
            pAxisEvents->second[static_cast<unsigned int>(EditorInputEventIds::Axis::MOVE_CAMERA_UP)] =
                [this](KeyboardModifiers modifiers, float input) { lastInputDirection.z = input; };
        }

        // Bind action events.
        {
            // Get action events.
            const auto pActionEvents = getActionEventBindings();
            std::scoped_lock guard(pActionEvents->first);

            // Bind increase speed.
            pActionEvents
                ->second[static_cast<unsigned int>(EditorInputEventIds::Action::INCREASE_CAMERA_SPEED)] =
                [this](KeyboardModifiers modifiers, bool bIsPressed) {
                    if (bIsPressed) {
                        currentMovementSpeedMultiplier = speedIncreaseMultiplier;
                        return;
                    }
                    currentMovementSpeedMultiplier = 1.0F;
                };

            // Bind decrease speed.
            pActionEvents
                ->second[static_cast<unsigned int>(EditorInputEventIds::Action::DECREASE_CAMERA_SPEED)] =
                [this](KeyboardModifiers modifiers, bool bIsPressed) {
                    if (bIsPressed) {
                        currentMovementSpeedMultiplier = speedDecreaseMultiplier;
                        return;
                    }
                    currentMovementSpeedMultiplier = 1.0F;
                };
        }
    }

    void EditorCameraNode::setIgnoreInput(bool bIgnore) {
        bIgnoreInput = bIgnore;

        setIsReceivingInput(!bIgnore);

        if (bIgnore) {
            // Reset any previous input (for ex. if the user was holding some button).
            lastInputDirection = glm::vec3(0.0F, 0.0F, 0.0F);
            currentMovementSpeedMultiplier = 1.0F;
        }
    }

    void EditorCameraNode::onBeforeNewFrame(float timeSincePrevFrameInSec) {
        CameraNode::onBeforeNewFrame(timeSincePrevFrameInSec);

        // Check for early exit.
        if (bIgnoreInput ||
            glm::all(glm::epsilonEqual(lastInputDirection, glm::vec3(0.0F, 0.0F, 0.0F), 0.0001F))) { // NOLINT
            return;
        }

        // Normalize direction to avoid speed up on diagonal movement and apply speed.
        const auto movementDirection = glm::normalize(lastInputDirection) * timeSincePrevFrameInSec *
                                       movementSpeed * currentMovementSpeedMultiplier;

        // Get node's world location.
        auto newWorldLocation = getWorldLocation();

        // Calculate new world location.
        newWorldLocation += getWorldForwardDirection() * movementDirection.x;
        newWorldLocation += getWorldRightDirection() * movementDirection.y;
        newWorldLocation += Globals::WorldDirection::up * movementDirection.z;

        // Apply movement.
        setWorldLocation(newWorldLocation);
    }

    void EditorCameraNode::onMouseMove(double xOffset, double yOffset) {
        // Although we disable node's "receiving input" when we are told to,
        // that event just queues a deferred task and until that task is not processed
        // we still receive input but the mouse cursor is shows which may cause unwanted rotations.
        if (bIgnoreInput) {
            return;
        }

        // Modify rotation.
        auto currentRotation = getRelativeRotation();
        currentRotation.z += static_cast<float>(xOffset * rotationSensitivity);
        currentRotation.y -= static_cast<float>(yOffset * rotationSensitivity);

        // Apply rotation.
        setRelativeRotation(currentRotation);
    }

    void EditorCameraNode::onAfterAttachedToNewParent(bool bThisNodeBeingAttached) {
        CameraNode::onAfterAttachedToNewParent(bThisNodeBeingAttached);

        // Make sure we don't have a spatial node in our parent chain
        // so that nothing will affect our movement/rotation.
        const auto pMtxSpatialParent = getClosestSpatialParent();
        std::scoped_lock guard(pMtxSpatialParent->first);

        if (pMtxSpatialParent->second != nullptr) [[unlikely]] {
            Error error(std::format(
                "editor camera node was attached to some node (tree) and there is now a "
                "spatial node \"{}\" in the editor camera's parent chain but having a spatial node "
                "in the editor camera's parent chain might cause the camera to move/rotate according "
                "to the parent (which is undesirable)",
                pMtxSpatialParent->second->getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }
}
