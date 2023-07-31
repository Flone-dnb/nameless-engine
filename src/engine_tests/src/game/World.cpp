// Custom.
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/nodes/Node.h"
#include "misc/ProjectPaths.h"
#include "../io/ReflectionTest.h"

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
        virtual void onSpawning() override {
            bWasSpawned = true;

            // Get root node.
            pRootNode = getWorldRootNode();
            REQUIRE(pRootNode);
        }
        virtual void onDespawning() override { bWasDespawned = true; }

    private:
        gc<Node> pRootNode;
        gc<Node> pMyChildChildNode;
        bool bWasSpawned = false;
        bool bWasDespawned = false;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                REQUIRE(getWorldRootNode());

                const auto pNode1 = gc_new<MyNode>();
                const auto pNode2 = gc_new<Node>();
                getWorldRootNode()->addChildNode(pNode1);
                getWorldRootNode()->addChildNode(pNode2);

                getWindow()->close();
            });
        }
        virtual ~TestGameInstance() override {}
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

TEST_CASE("test world time") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                const auto pTimer = createTimer("test world time");
                REQUIRE(pTimer != nullptr);

                constexpr float timeToWaitInSec = 1.25F;
                pTimer->setCallbackForTimeout(static_cast<long long>(timeToWaitInSec * 1000), [this]() {
                    const auto worldTimeInSec = getWorldTimeInSeconds();

                    const auto diffInSec = std::abs(worldTimeInSec - timeToWaitInSec);
                    REQUIRE(diffInSec < 0.1F);

                    getWindow()->close();
                });

                pTimer->start();
            });
        }
        virtual ~TestGameInstance() override {}
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

TEST_CASE("create world and switch to another world") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            // Create initial world.
            createWorld([&](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                REQUIRE(getWorldRootNode());

                const auto pNode = gc_new<Node>();
                getWorldRootNode()->addChildNode(pNode);

                // Create another world now.
                createWorld([&](const std::optional<Error>& optionalError) {
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }

                    const auto pNode = gc_new<Node>();
                    getWorldRootNode()->addChildNode(pNode);

                    // Finished.
                    getWindow()->close();
                });
            });
        }
        virtual ~TestGameInstance() override {}
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

TEST_CASE("create, serialize and deserialize world") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Add child node.
                const auto pMyNode = gc_new<ReflectionTestNode1>();
                REQUIRE(!pMyNode->bBoolValue2);
                pMyNode->bBoolValue2 = true;
                getWorldRootNode()->addChildNode(pMyNode);

                // And another child node.
                const auto pChildNode = gc_new<Node>();
                pMyNode->addChildNode(pChildNode);

                // Serialize world.
                auto optionalSerializationError =
                    getWorldRootNode()->serializeNodeTree(fullPathToNodeTreeFile, false);
                if (optionalSerializationError.has_value()) {
                    auto error = optionalSerializationError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Remove existing world.
                createWorld([&](const std::optional<Error>& optionalWorldError) {
                    if (optionalWorldError.has_value()) {
                        auto error = optionalWorldError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }

                    // Deserialize world.
                    loadNodeTreeAsWorld(
                        [&](const std::optional<Error>& optionalWorldError) {
                            if (optionalWorldError.has_value()) {
                                auto error = optionalWorldError.value();
                                error.addCurrentLocationToErrorStack();
                                INFO(error.getFullErrorMessage());
                                REQUIRE(false);
                            }
                            // Check that everything is correct.
                            REQUIRE(getWorldRootNode());
                            REQUIRE(getWorldRootNode()->getChildNodes()->second->size() == 1);

                            auto pMyNode = gc_dynamic_pointer_cast<ReflectionTestNode1>(
                                getWorldRootNode()->getChildNodes()->second->operator[](0));
                            REQUIRE(pMyNode);
                            REQUIRE(pMyNode->bBoolValue2);
                            REQUIRE(pMyNode->getChildNodes()->second->size() == 1);

                            getWindow()->close();
                        },
                        fullPathToNodeTreeFile);
                });
            });
        }
        virtual ~TestGameInstance() override {}

    private:
        const std::filesystem::path fullPathToNodeTreeFile =
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
            "TESTING_TestWorld_TESTING.toml";
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

TEST_CASE("check that node is spawned") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                REQUIRE(getWorldRootNode());

                const auto pNode = gc_new<Node>();
                getWorldRootNode()->addChildNode(pNode);

                // Check that spawned.
                REQUIRE(pNode->getNodeId().has_value());
                REQUIRE(GameManager::get()->isNodeSpawned(pNode->getNodeId().value()));

                pNode->detachFromParentAndDespawn();

                // Check that despawned.
                REQUIRE(!GameManager::get()->isNodeSpawned(pNode->getNodeId().value()));

                getWindow()->close();
            });
        }
        virtual ~TestGameInstance() override {}
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
