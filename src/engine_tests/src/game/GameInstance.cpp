// Custom.
#include "game/GameInstance.h"
#include "game/Window.h"
#include "misc/Timer.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("input event callbacks in GameInstance are triggered") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addCurrentLocationToErrorStack();
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                optionalError =
                    getInputManager()->addAxisEvent("axis1", {{KeyboardKey::KEY_A, KeyboardKey::KEY_B}});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
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
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("timer callback is called") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override { REQUIRE(bCallbackCalled); }
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                constexpr long long iWaitTime = 40;
                constexpr size_t iCheckIntervalTimeInMs = 20;
                using namespace std::chrono;

                // Create timer.
                pTimer = createTimer("test timer");
                REQUIRE(pTimer != nullptr);

                // This will queue a deferred task on timeout.
                pTimer->setCallbackForTimeout(iWaitTime, [this]() {
                    bCallbackCalled = true;
                    getWindow()->close();
                });

                pTimer->start();
                std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

                REQUIRE(pTimer->isRunning());
                REQUIRE(!pTimer->isStopped());

                std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime * 2));

                // Timeout should submit a deferred task at this point.
                REQUIRE(!pTimer->isRunning());
                REQUIRE(!pTimer->isStopped());
            });
        }

    protected:
        Timer* pTimer = nullptr;
        bool bCallbackCalled = false;
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("timer callback validator prevents callback to be called after onWindowClose was started") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override = default;
        virtual void onGameStarted() override {
            constexpr long long iWaitTime = 40;
            constexpr size_t iCheckIntervalTimeInMs = 20;
            using namespace std::chrono;

            // Create timer.
            pTimer = createTimer("test timer");
            REQUIRE(pTimer != nullptr);

            // This will queue a deferred task on timeout.
            pTimer->setCallbackForTimeout(iWaitTime, []() {
                // This should not be called.
                REQUIRE(false);
            });

            pTimer->start();
            std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

            REQUIRE(pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());

            std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime * 2));

            // Timeout should submit a deferred task at this point.
            REQUIRE(!pTimer->isRunning());
            REQUIRE(!pTimer->isStopped());

            getWindow()->close();
        }

    protected:
        Timer* pTimer = nullptr;
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("timer callback validator prevents old (stopped) callback from being called") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override { REQUIRE(bExpectedCallbackWasCalled); };
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                constexpr long long iWaitTime = 50;
                constexpr size_t iCheckIntervalTimeInMs = 20;
                using namespace std::chrono;

                // Create timer.
                pTimer = createTimer("test timer");
                REQUIRE(pTimer != nullptr);

                // This will queue a deferred task on timeout.
                pTimer->setCallbackForTimeout(iWaitTime, []() {
                    // This should not be called.
                    REQUIRE(false);
                });

                pTimer->start();
                std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

                REQUIRE(pTimer->isRunning());
                REQUIRE(!pTimer->isStopped());

                std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime * 2));

                // Timeout should submit a deferred task at this point.
                REQUIRE(!pTimer->isRunning());
                REQUIRE(!pTimer->isStopped());

                // Start a new one.
                pTimer->setCallbackForTimeout(iWaitTime, [this]() {
                    REQUIRE(true);
                    bExpectedCallbackWasCalled = true;
                    getWindow()->close();
                });

                pTimer->start();

                std::this_thread::sleep_for(std::chrono::milliseconds(iCheckIntervalTimeInMs));

                REQUIRE(pTimer->isRunning());
                REQUIRE(!pTimer->isStopped());

                std::this_thread::sleep_for(std::chrono::milliseconds(iWaitTime * 2));

                // Timeout should submit a deferred task at this point.
                REQUIRE(!pTimer->isRunning());
                REQUIRE(!pTimer->isStopped());
            });
        }

    protected:
        Timer* pTimer = nullptr;
        bool bExpectedCallbackWasCalled = false;
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}
