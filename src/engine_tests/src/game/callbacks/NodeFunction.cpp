// Custom.
#include "game/nodes/Node.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/callbacks/NodeFunction.hpp"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("node callback function is not called after the node is despawned") {
    using namespace ne;

    class MyNode : public Node {
    public:
        NodeFunction<void(bool)> getCallback() {
            return NodeFunction<void(bool)>(
                getNodeId().value(), [this](bool bShouldClose) { myCallback(bShouldClose); });
        }

    private:
        void myCallback(bool bShouldClose) {
            if (bShouldClose) {
                // We should not get here.
                REQUIRE(false);
            } else {
                sSomePrivateString = "Seems to work, should close: false.";
            }
        }

        std::string sSomePrivateString = "Hello!";
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                auto pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode);

                // Save callback.
                auto callback = pMyNode->getCallback();

                // Test callback.
                REQUIRE(callback.isNodeSpawned());
                REQUIRE(!callback(false));

                // Despawn node.
                pMyNode->detachFromParentAndDespawn();

                // Test callback again.
                REQUIRE(!callback.isNodeSpawned());
                REQUIRE(callback(true));

                getWindow()->close();
            });
        }
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
