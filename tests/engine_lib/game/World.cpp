// Custom.
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/nodes/Node.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("create and destroy world") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() {
            const auto pChildNode1 = tgc2::gc_new<Node>("Child Node 1");
            addChildNode(pChildNode1);

            auto pChildNode2 = tgc2::gc_new<Node>("Child Node 2");
            const auto pChildChildNode = tgc2::gc_new<Node>("Child Child Node");
            pChildNode2->addChildNode(pChildChildNode);
            addChildNode(pChildNode2);

            // Get child node.
            pMyChildChildNode = getChildNodeOfType<Node>("Child Child Node");
            REQUIRE(pMyChildChildNode);
        }
        virtual ~MyNode() override {
            REQUIRE(bWasSpawned);
            REQUIRE(bWasDespawned);
        }

    protected:
        virtual void onSpawn() override {
            bWasSpawned = true;

            // Get root node.
            pRootNode = getWorldRootNode();
            REQUIRE(pRootNode);
        }
        virtual void onDespawn() override { bWasDespawned = true; }

    private:
        gc<Node> pRootNode;
        gc<Node> pMyChildChildNode;
        bool bWasSpawned = false;
        bool bWasDespawned = false;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld();

            REQUIRE(getWorldRootNode());

            const auto pNode1 = gc_new<MyNode>();
            const auto pNode2 = gc_new<Node>();
            getWorldRootNode()->addChildNode(pNode1);
            getWorldRootNode()->addChildNode(pNode2);

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("create world and switch to another world") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            // Create initial world.
            createWorld();

            REQUIRE(getWorldRootNode());

            {
                const auto pNode = gc_new<Node>();
                getWorldRootNode()->addChildNode(pNode);
            }

            // Create another world now.
            createWorld();

            {
                const auto pNode = gc_new<Node>();
                getWorldRootNode()->addChildNode(pNode);
            }

            // Finished.
            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
