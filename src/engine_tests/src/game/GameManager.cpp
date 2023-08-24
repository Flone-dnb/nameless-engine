// Custom.
#include "game/GameManager.h"
#include "game/Window.h"
#include "game/GameInstance.h"
#include "game/nodes/Node.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("deferred task queue an additional deferred task - both executed at once") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            addDeferredTask([this]() {
                // This task is executed on the next game tick.
                const auto iCurrentTickCount = iTickCount;
                addDeferredTask([this, iCurrentTickCount]() {
                    // This task should be executed on the same game tick.
                    REQUIRE(iTickCount == iCurrentTickCount);
                    getWindow()->close();
                });
            });
        }
        virtual ~TestGameInstance() override = default;

    protected:
        virtual void onBeforeNewFrame(float timeSincePrevCall) override {
            GameInstance::onBeforeNewFrame(timeSincePrevCall);
            iTickCount += 1;
        }

    private:
        size_t iTickCount = 0;
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

TEST_CASE("create world in deferred task during game destruction") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            getWindow()->close();
            addDeferredTask([this]() {
                createWorld(
                    [](const std::optional<Error>& optionalError) { REQUIRE(!optionalError.has_value()); });
            });
        }
        virtual ~TestGameInstance() override = default;
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
