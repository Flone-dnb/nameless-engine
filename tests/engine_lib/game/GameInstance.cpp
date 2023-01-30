// Custom.
#include "game/GameInstance.h"
#include "game/Window.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("input event callbacks in GameInstance are triggered") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Bind to events.
                {
                    const auto pActionEvents = getActionEventBindings();
                    std::scoped_lock guard(pActionEvents->first);

                    pActionEvents->second["action1"] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
                        action1(modifiers, bIsPressedDown);
                    };
                }

                {
                    const auto pAxisEvents = getAxisEventBindings();
                    std::scoped_lock guard(pAxisEvents->first);

                    pAxisEvents->second["axis1"] = [&](KeyboardModifiers modifiers, float input) {
                        axis1(modifiers, input);
                    };
                }

                // Register events.
                auto optionalError = getInputManager()->addActionEvent("action1", {KeyboardKey::KEY_W});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                optionalError =
                    getInputManager()->addAxisEvent("axis1", {{KeyboardKey::KEY_A, KeyboardKey::KEY_B}});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Simulate input.
                getWindow()->onKeyboardInput(KeyboardKey::KEY_A, KeyboardModifiers(0), true);
                getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);

                REQUIRE(bAction1Triggered);
                REQUIRE(bAxis1Triggered);

                getWindow()->close();
            });
        }
        virtual ~TestGameInstance() override {}

    private:
        bool bAction1Triggered = false;
        bool bAxis1Triggered = false;

        void action1(KeyboardModifiers modifiers, bool bIsPressedDown) { bAction1Triggered = true; }
        void axis1(KeyboardModifiers modifiers, float input) { bAxis1Triggered = true; }
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
