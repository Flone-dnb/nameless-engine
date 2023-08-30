#pragma once

// Custom.
#include "game/GameInstance.h"

namespace ne {
    class Window;
    class GameManager;
    class TransientCamera;
    class InputManager;

    /** Defines editor game. */
    class EditorGameInstance : public GameInstance {
    public:
        /**
         * Returns title of the editor's window.
         *
         * @return Window title.
         */
        static const char* getEditorWindowTitle();

        /**
         * Constructor.
         *
         * @remark There is no need to save window/input manager pointers
         * in derived classes as the base class already saves these pointers and
         * provides @ref getWindow and @ref getInputManager functions.
         *
         * @param pWindow       Window that owns this game instance.
         * @param pGameManager  GameManager that owns this game instance.
         * @param pInputManager Input manager of the owner Game object.
         */
        EditorGameInstance(Window* pWindow, GameManager* pGameManager, InputManager* pInputManager);

        /**
         * Returns camera that's used for editor's viewport.
         *
         * @return Camera.
         */
        std::shared_ptr<TransientCamera> getEditorCamera();

        virtual ~EditorGameInstance() override = default;

    protected:
        /**
         * Called after GameInstance's constructor is finished and created
         * GameInstance object was saved in GameManager (that owns GameInstance).
         *
         * At this point you can create and interact with the game world and etc.
         */
        virtual void onGameStarted() override;

        /**
         * Called when the window received mouse movement.
         *
         * @param iXOffset  Mouse X movement delta in pixels (plus if moved to the right,
         * minus if moved to the left).
         * @param iYOffset  Mouse Y movement delta in pixels (plus if moved up,
         * minus if moved down).
         */
        virtual void onMouseMove(int iXOffset, int iYOffset) override;

        /**
         * Called before a new frame is rendered.
         *
         * @remark Called before nodes that should be called every frame.
         *
         * @param timeSincePrevCallInSec Time in seconds that has passed since the last call
         * to this function.
         */
        virtual void onBeforeNewFrame(float timeSincePrevCallInSec) override;

    private:
        /** Stores unique IDs of input events. */
        struct InputEventIds {
            /** Groups action events. */
            struct Action {
                /** ID of the action event for capturing mouse cursor. */
                static constexpr unsigned int iCaptureMouseCursor = 0;

                /** ID of the action event for increasing camera's speed. */
                static constexpr unsigned int iIncreaseCameraSpeed = 1;

                /** ID of the action event for decreasing camera's speed. */
                static constexpr unsigned int iDecreaseCameraSpeed = 2;
            };

            /** Groups axis events. */
            struct Axis {
                /** ID of the axis event for moving camera forward. */
                static constexpr unsigned int iMoveForward = 0;

                /** ID of the axis event for moving camera right. */
                static constexpr unsigned int iMoveRight = 1;

                /** ID of the axis event for moving camera up. */
                static constexpr unsigned int iMoveUp = 2;
            };
        };

        /** Binds input events to control @ref pEditorCamera. */
        void bindCameraInput();

        /** Updates @ref pEditorCamera speed based on the current settings. */
        void updateCameraSpeed();

        /** Camera used in the editor. */
        std::shared_ptr<TransientCamera> pEditorCamera;

        /** @ref pEditorCamera movement speed. */
        float cameraMovementSpeed = 3.0F; // NOLINT: default value

        /** Rotation multiplier for @ref pEditorCamera. */
        float cameraRotationSensitivity = 0.1F; // NOLINT: default value

        /** Determines whether @ref cameraSpeedIncreaseMultiplier should be used or not. */
        bool bShouldIncreaseCameraSpeed = false;

        /** Determines whether @ref cameraSpeedDecreaseMultiplier should be used or not. */
        bool bShouldDecreaseCameraSpeed = false;

        /** Determines whether we can move and rotate @ref pEditorCamera or not. */
        bool bIsMouseCursorCaptured = false;

        /** Camera speed multiplier when fast movement mode is enabled (for ex. Shift is pressed). */
        const float cameraSpeedIncreaseMultiplier = 2.0F;

        /** Camera speed multiplier when slow movement mode is enabled (for ex. Ctrl is pressed). */
        const float cameraSpeedDecreaseMultiplier = 0.5F;

        /** Title of the editor's window. */
        static constexpr auto pEditorWindowTitle = "Nameless Editor";
    };
} // namespace ne
