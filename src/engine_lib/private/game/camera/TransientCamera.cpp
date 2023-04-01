#include "game/camera/TransientCamera.h"

namespace ne {

    void TransientCamera::setFreeCameraForwardMovement(float input) {
        lastInputDirection.x = std::clamp(input, -1.0F, 1.0F);

        // Stop input processing if the input is zero.
        if (glm::all(glm::epsilonEqual(lastInputDirection, glm::vec3(0.0F, 0.0F, 0.0F), inputDelta))) {
            bHasInputToProcess = false;
            return;
        }

        // Normalize in order to avoid speed boost when multiple input keys are pressed.
        lastInputDirection = glm::normalize(lastInputDirection);

        // Mark input to be processed.
        bHasInputToProcess = true;
    }

    void TransientCamera::setFreeCameraRightMovement(float input) {
        lastInputDirection.y = std::clamp(input, -1.0F, 1.0F);

        // Stop input processing if the input is zero.
        if (glm::all(glm::epsilonEqual(lastInputDirection, glm::vec3(0.0F, 0.0F, 0.0F), inputDelta))) {
            bHasInputToProcess = false;
            return;
        }

        // Normalize in order to avoid speed boost when multiple input keys are pressed.
        lastInputDirection = glm::normalize(lastInputDirection);

        // Mark input to be processed.
        bHasInputToProcess = true;
    }

    void TransientCamera::setFreeCameraWorldUpMovement(float input) {
        lastInputDirection.z = std::clamp(input, -1.0F, 1.0F);

        // Stop input processing if the input is zero.
        if (glm::all(glm::epsilonEqual(lastInputDirection, glm::vec3(0.0F, 0.0F, 0.0F), inputDelta))) {
            bHasInputToProcess = false;
            return;
        }

        // Normalize in order to avoid speed boost when multiple input keys are pressed.
        lastInputDirection = glm::normalize(lastInputDirection);

        // Mark input to be processed.
        bHasInputToProcess = true;
    }

    CameraProperties* TransientCamera::getCameraProperties() { return &properties; }

    void TransientCamera::onBeforeNewFrame(float timeSincePrevCallInSec) {
        // Make sure the input is not zero.
        if (!bHasInputToProcess) {
            return;
        }

        properties.moveFreeCameraForward(lastInputDirection.x * timeSincePrevCallInSec);
        properties.moveFreeCameraRight(lastInputDirection.y * timeSincePrevCallInSec);
        properties.moveFreeCameraWorldUp(lastInputDirection.z * timeSincePrevCallInSec);
    }

    void TransientCamera::clearInput() { lastInputDirection = glm::vec3(0.0F, 0.0F, 0.0F); }

} // namespace ne
