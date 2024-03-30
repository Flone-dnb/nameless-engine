#pragma once

// Custom.
#include "game/GameInstance.h"

// External.
#include "GcPtr.h"

namespace ne {
    class Window;
    class GameManager;
    class EditorCameraNode;

    /**
     * Defines editor game.
     *
     * @remark This node expects to be a child of the world's root node so that parent rotations will not
     * affect the camera.
     */
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
         * @return Camera node.
         */
        sgc::GcPtr<EditorCameraNode> getEditorCamera() const;

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
         * Called before a new frame is rendered.
         *
         * @remark Called before nodes that should be called every frame.
         *
         * @param timeSincePrevCallInSec Time in seconds that has passed since the last call
         * to this function.
         */
        virtual void onBeforeNewFrame(float timeSincePrevCallInSec) override;

    private:
        /** Groups all GC pointers that the editor holds. */
        struct EditorGcPointers {
            /** Camera used in the editor. */
            sgc::GcPtr<EditorCameraNode> pCameraNode;
        };

        /** Called after a new world was created to create editor-specific nodes such as camera and etc. */
        void spawnEditorNodesForNewWorld();

        /** All GC pointer that the editor holds. */
        EditorGcPointers gcPointers;

        /** Title of the editor's window. */
        static constexpr auto pEditorWindowTitle = "Nameless Editor";
    };
} // namespace ne
