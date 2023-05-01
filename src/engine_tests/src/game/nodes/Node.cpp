// Custom.
#include "game/nodes/Node.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "misc/Timer.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("node names should not be unique") {
    using namespace ne;

    const auto sNodeName = "Test Node Name";

    const auto pNode1 = gc_new<Node>(sNodeName);
    const auto pNode2 = gc_new<Node>(sNodeName);

    REQUIRE(pNode1->getNodeName() == sNodeName);
    REQUIRE(pNode2->getNodeName() == sNodeName);
}

TEST_CASE("build and check node hierarchy") {
    using namespace ne;

    {
        // Create nodes.
        const auto pParentNode = gc_new<Node>();
        const auto pChildNode = gc_new<Node>();

        const auto pChildChildNode1 = gc_new<Node>();
        const auto pChildChildNode2 = gc_new<Node>();

        // Build hierarchy.
        pChildNode->addChildNode(pChildChildNode1);
        pChildNode->addChildNode(pChildChildNode2);
        pParentNode->addChildNode(pChildNode);

        const auto pMtxParentChildNodes = pParentNode->getChildNodes();
        std::scoped_lock parentChildNodesGuard(pMtxParentChildNodes->first);

        const auto pMtxChildChildNodes = pChildNode->getChildNodes();
        std::scoped_lock childChildNodesGuard(pMtxChildChildNodes->first);

        // Check that everything is correct.
        REQUIRE(pMtxParentChildNodes->second->size() == 1);
        REQUIRE(&*pMtxParentChildNodes->second->operator[](0) == &*pChildNode);

        REQUIRE(pMtxChildChildNodes->second->size() == 2);
        REQUIRE(&*pMtxChildChildNodes->second->operator[](0) == &*pChildChildNode1);
        REQUIRE(&*pMtxChildChildNodes->second->operator[](1) == &*pChildChildNode2);

        REQUIRE(pChildNode->getParentNode()->second == pParentNode);
        REQUIRE(pChildChildNode1->getParentNode()->second == pChildNode);
        REQUIRE(pChildChildNode2->getParentNode()->second == pChildNode);

        REQUIRE(pParentNode->isParentOf(&*pChildNode));
        REQUIRE(pParentNode->isParentOf(&*pChildChildNode1));
        REQUIRE(pParentNode->isParentOf(&*pChildChildNode2));

        REQUIRE(pChildNode->isChildOf(&*pParentNode));
        REQUIRE(pChildChildNode1->isChildOf(&*pParentNode));
        REQUIRE(pChildChildNode1->isChildOf(&*pChildNode));
        REQUIRE(pChildChildNode2->isChildOf(&*pParentNode));
        REQUIRE(pChildChildNode2->isChildOf(&*pChildNode));

        REQUIRE(!pChildChildNode1->isChildOf(&*pChildChildNode2));
        REQUIRE(!pChildChildNode1->isParentOf(&*pChildChildNode2));
    }

    // Cleanup.
    gc_collector()->collect();
    REQUIRE(Node::getAliveNodeCount() == 0);
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("move nodes in the hierarchy") {
    using namespace ne;

    {
        // Create nodes.
        const auto pParentNode = gc_new<Node>();
        const auto pCharacterNode = gc_new<Node>();
        const auto pCarNode = gc_new<Node>();
        const auto pSomeNode = gc_new<Node>();

        const auto pCharacterChildNode1 = gc_new<Node>();
        const auto pCharacterChildNode2 = gc_new<Node>();

        // Build hierarchy.
        pCharacterNode->addChildNode(pCharacterChildNode1);
        pCharacterNode->addChildNode(pCharacterChildNode2);
        pParentNode->addChildNode(pCharacterNode);
        pParentNode->addChildNode(pCarNode);

        // Attach the character to the car.
        pCarNode->addChildNode(pCharacterNode);
        pCarNode->addChildNode(pSomeNode);

        // Check that everything is correct.
        REQUIRE(&*pCharacterNode->getParentNode()->second == &*pCarNode);
        REQUIRE(&*pSomeNode->getParentNode()->second == &*pCarNode);
        REQUIRE(pCharacterNode->getChildNodes()->second->size() == 2);
        REQUIRE(pCarNode->getChildNodes()->second->size() == 2);
        REQUIRE(pCharacterChildNode1->isChildOf(&*pCharacterNode));
        REQUIRE(pCharacterChildNode2->isChildOf(&*pCharacterNode));

        // Detach some node.
        pSomeNode->detachFromParentAndDespawn();
        REQUIRE(pSomeNode->getParentNode()->second == nullptr);

        REQUIRE(pCarNode->getChildNodes()->second->size() == 1);

        // Detach the character from the car.
        pParentNode->addChildNode(pCharacterNode);

        // Check that everything is correct.
        REQUIRE(&*pCharacterNode->getParentNode()->second == &*pParentNode);
        REQUIRE(pCharacterNode->getChildNodes()->second->size() == 2);
        REQUIRE(pCharacterChildNode1->isChildOf(&*pCharacterNode));
        REQUIRE(pCharacterChildNode2->isChildOf(&*pCharacterNode));
    }

    // Cleanup.
    gc_collector()->collect();
    REQUIRE(Node::getAliveNodeCount() == 0);
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("serialize and deserialize node tree") {
    using namespace ne;

    // Prepare paths.
    const std::filesystem::path pathToFile = ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                                             "test" / "temp" /
                                             "TESTING_NodeTree_TESTING"; // not specifying ".toml" on purpose
    const std::filesystem::path fullPathToFile =
        ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
        "TESTING_NodeTree_TESTING.toml";

    {
        // Create nodes.
        const auto pRootNode = gc_new<Node>("Root Node");
        const auto pChildNode1 = gc_new<Node>("Child Node 1");
        const auto pChildNode2 = gc_new<Node>("Child Node 2");
        const auto pChildChildNode1 = gc_new<Node>("Child Child Node 1");

        // Build hierarchy.
        pRootNode->addChildNode(pChildNode1);
        pRootNode->addChildNode(pChildNode2);
        pChildNode1->addChildNode(pChildChildNode1);

        // Serialize.
        const auto optionalError = pRootNode->serializeNodeTree(pathToFile, false);
        if (optionalError.has_value()) {
            auto err = optionalError.value();
            err.addEntry();
            INFO(err.getFullErrorMessage());
            REQUIRE(false);
        }

        REQUIRE(std::filesystem::exists(fullPathToFile));
    }

    gc_collector()->fullCollect();
    REQUIRE(Node::getAliveNodeCount() == 0); // cyclic references should be freed

    {
        // Deserialize.
        const auto deserializeResult = Node::deserializeNodeTree(pathToFile);
        if (std::holds_alternative<Error>(deserializeResult)) {
            auto err = std::get<Error>(deserializeResult);
            err.addEntry();
            INFO(err.getFullErrorMessage());
            REQUIRE(false);
        }
        const auto pRootNode = std::get<gc<Node>>(deserializeResult);

        // Check results.
        REQUIRE(pRootNode->getNodeName() == "Root Node");
        const auto pMtxChildNodes = pRootNode->getChildNodes();
        std::scoped_lock childNodesGuard(pMtxChildNodes->first);
        REQUIRE(pMtxChildNodes->second->size() == 2);

        // Check child nodes.
        gc<Node> pChildNode1;
        gc<Node> pChildNode2;
        if ((*pMtxChildNodes->second)[0]->getNodeName() == "Child Node 1") {
            REQUIRE((*pMtxChildNodes->second)[1]->getNodeName() == "Child Node 2");
            pChildNode1 = (*pMtxChildNodes->second)[0];
            pChildNode2 = (*pMtxChildNodes->second)[1];
        } else if ((*pMtxChildNodes->second)[0]->getNodeName() == "Child Node 2") {
            REQUIRE((*pMtxChildNodes->second)[1]->getNodeName() == "Child Node 1");
            pChildNode1 = (*pMtxChildNodes->second)[1];
            pChildNode2 = (*pMtxChildNodes->second)[2];
        }

        // Check for child child nodes.
        REQUIRE(pChildNode2->getChildNodes()->second->empty());
        const auto pMtxChildChildNodes = pChildNode1->getChildNodes();
        std::scoped_lock childChildNodesGuard(pMtxChildChildNodes->first);
        REQUIRE(pMtxChildChildNodes->second->size() == 1);
        REQUIRE((*pMtxChildChildNodes->second)[0]->getChildNodes()->second->empty());
        REQUIRE((*pMtxChildChildNodes->second)[0]->getNodeName() == "Child Child Node 1");
    }

    gc_collector()->fullCollect();
    REQUIRE(Node::getAliveNodeCount() == 0); // cyclic references should be freed
}

TEST_CASE("get parent node of type") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() = default;
        MyDerivedNode(const std::string& sName) : Node(sName) {}
        virtual ~MyDerivedNode() override = default;
        int iAnswer = 0;
    };

    class MyDerivedDerivedNode : public MyDerivedNode {
    public:
        MyDerivedDerivedNode() = default;
        MyDerivedDerivedNode(const std::string& sName) : MyDerivedNode(sName) {}
        virtual ~MyDerivedDerivedNode() override = default;
        virtual void onSpawning() override {
            MyDerivedNode::onSpawning();

            bSpawnCalled = true;

            // Get parent without name.
            auto pNode = getParentNodeOfType<MyDerivedNode>();
            REQUIRE(&*pNode == &*getParentNode()->second);
            REQUIRE(pNode->iAnswer == 0);

            // Get parent with name.
            pNode = getParentNodeOfType<MyDerivedNode>("MyDerivedNode");
            REQUIRE(pNode->iAnswer == 42);
        }
        bool bSpawnCalled = false;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create nodes.
                auto pDerivedNodeParent = gc_new<MyDerivedNode>("MyDerivedNode");
                pDerivedNodeParent->iAnswer = 42;

                const auto pDerivedNodeChild = gc_new<MyDerivedNode>();

                auto pDerivedDerivedNode = gc_new<MyDerivedDerivedNode>();

                // Build node hierarchy.
                pDerivedNodeChild->addChildNode(pDerivedDerivedNode);
                pDerivedNodeParent->addChildNode(pDerivedNodeChild);
                getWorldRootNode()->addChildNode(pDerivedNodeParent);

                REQUIRE(pDerivedDerivedNode->bSpawnCalled);

                getWindow()->close();
            });
        }
        virtual ~TestGameInstance() override {}
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("get child node of type") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() = default;
        MyDerivedNode(const std::string& sName) : Node(sName) {}
        virtual ~MyDerivedNode() override = default;
        int iAnswer = 0;
    };

    class MyDerivedDerivedNode : public MyDerivedNode {
    public:
        MyDerivedDerivedNode() = default;
        MyDerivedDerivedNode(const std::string& sName) : MyDerivedNode(sName) {}
        virtual ~MyDerivedDerivedNode() override = default;
        virtual void onSpawning() override {
            MyDerivedNode::onSpawning();

            bSpawnCalled = true;

            // Get child without name.
            auto pNode = getChildNodeOfType<MyDerivedNode>();
            REQUIRE(&*pNode == &*getChildNodes()->second->operator[](0));
            REQUIRE(pNode->iAnswer == 0);

            // Get child with name.
            pNode = getChildNodeOfType<MyDerivedNode>("MyDerivedNode");
            REQUIRE(pNode->iAnswer == 42);
        }
        bool bSpawnCalled = false;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                // Create nodes.
                auto pDerivedDerivedNode = gc_new<MyDerivedDerivedNode>();

                auto pDerivedNodeParent = gc_new<MyDerivedNode>();

                const auto pDerivedNodeChild = gc_new<MyDerivedNode>("MyDerivedNode");
                pDerivedNodeChild->iAnswer = 42;

                // Build node hierarchy.
                pDerivedNodeParent->addChildNode(pDerivedNodeChild);
                pDerivedDerivedNode->addChildNode(pDerivedNodeParent);
                getWorldRootNode()->addChildNode(pDerivedDerivedNode);

                REQUIRE(pDerivedDerivedNode->bSpawnCalled);

                getWindow()->close();
            });
        }
        virtual ~TestGameInstance() override {}
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("saving pointer to the root node does not prevent correct world destruction") {
    using namespace ne;

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() = default;
        virtual ~MyDerivedNode() override = default;

        gc<Node> pRootNode = nullptr;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create our custom node.
                auto pNode = gc_new<MyDerivedNode>();
                pNode->pRootNode = getWorldRootNode();
                REQUIRE(pNode->pRootNode);

                // At this point the pointer to the root node is stored in two places:
                // - in World object,
                // - in our custom node.
                getWorldRootNode()->addChildNode(pNode);

                // Change world to see if GC will collect everything.
                createWorld([&](const std::optional<Error>& optionalWorldError) {
                    if (optionalWorldError.has_value()) {
                        auto error = optionalWorldError.value();
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    getWindow()->close();
                });
            });
        }
        virtual ~TestGameInstance() override {}
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

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("test GC performance and stability with nodes") {
    // This test is needed because the original version of our garbage collector
    // had a bug (that I fixed) that was crashing the program when we had
    // around 6000-8000 nodes.
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([](const std::optional<Error>&) {});
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (Node::getAliveNodeCount() == 10000) {
                getWindow()->close();
                return;
            }

            const auto pNewNode = gc_new<Node>();
            addChildNodes(100, pNewNode);
            getWorldRootNode()->addChildNode(pNewNode);
        }
        virtual ~TestGameInstance() override {}

    private:
        void addChildNodes(size_t iChildrenCount, gc<Node> pNode) {
            using namespace ne;

            if (iChildrenCount == 0)
                return;

            const auto pNewNode = gc_new<Node>();
            addChildNodes(iChildrenCount - 1, pNewNode);
            pNode->addChildNode(pNewNode);
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("onBeforeNewFrame is called only on marked nodes") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode(bool bEnableTick) { setIsCalledEveryFrame(bEnableTick); }

        bool bTickCalled = false;

    protected:
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            Node::onBeforeNewFrame(fTimeSincePrevCallInSec);

            bTickCalled = true;
        }
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                REQUIRE(getWorldRootNode());

                REQUIRE(getCalledEveryFrameNodeCount() == 0);

                pNotCalledtNode = gc_new<MyNode>(false);
                getWorldRootNode()->addChildNode(pNotCalledtNode); // queues deferred task to add to world

                pCalledNode = gc_new<MyNode>(true);
                getWorldRootNode()->addChildNode(pCalledNode); // queues deferred task to add to world
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            iTicks += 1;

            if (iTicks == 2) {
                REQUIRE(getTotalSpawnedNodeCount() == 3);
                REQUIRE(getCalledEveryFrameNodeCount() == 1);

                REQUIRE(pCalledNode->bTickCalled);
                REQUIRE(!pNotCalledtNode->bTickCalled);

                getWindow()->close();
            }
        }

    private:
        size_t iTicks = 0;
        gc<MyNode> pCalledNode;
        gc<MyNode> pNotCalledtNode;
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

TEST_CASE("tick groups order is correct") {
    using namespace ne;

    class MyFirstNode;
    class MySecondNode;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                REQUIRE(getWorldRootNode());

                getWorldRootNode()->addChildNode(gc_new<MyFirstNode>());
                getWorldRootNode()->addChildNode(gc_new<MySecondNode>());
            });
        }
        virtual ~TestGameInstance() override {}

        void onFirstNodeTick() {
            bFirstNodeCalled = true;
            REQUIRE(!bSecondNodeCalled);
        }

        void onSecondNodeTick() {
            bSecondNodeCalled = true;
            REQUIRE(bFirstNodeCalled);

            getWindow()->close();
        }

        virtual void onWindowClose() override {
            REQUIRE(bFirstNodeCalled);
            REQUIRE(bSecondNodeCalled);
        }

    private:
        bool bFirstNodeCalled = false;
        bool bSecondNodeCalled = false;
    };

    class MyFirstNode : public Node {
    public:
        MyFirstNode() { setIsCalledEveryFrame(true); }

    protected:
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            Node::onBeforeNewFrame(fTimeSincePrevCallInSec);

            dynamic_cast<TestGameInstance*>(getGameInstance())->onFirstNodeTick();
        }
    };

    class MySecondNode : public Node {
    public:
        MySecondNode() {
            setIsCalledEveryFrame(true);
            setTickGroup(TickGroup::SECOND);
        }

    protected:
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            Node::onBeforeNewFrame(fTimeSincePrevCallInSec);

            dynamic_cast<TestGameInstance*>(getGameInstance())->onSecondNodeTick();
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

TEST_CASE("input event callbacks in Node are triggered") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() {
            setReceiveInput(true);

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
        }

        bool bAction1Triggered = false;
        bool bAxis1Triggered = false;

    private:
        void action1(KeyboardModifiers modifiers, bool bIsPressedDown) { bAction1Triggered = true; }
        void axis1(KeyboardModifiers modifiers, float input) { bAxis1Triggered = true; }
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world

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
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            // Simulate input.
            getWindow()->onKeyboardInput(KeyboardKey::KEY_A, KeyboardModifiers(0), true);
            getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);

            REQUIRE(pMyNode->bAction1Triggered);
            REQUIRE(pMyNode->bAxis1Triggered);

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}

    private:
        gc<MyNode> pMyNode;
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

TEST_CASE("use deferred task with node's member function while the world is being changed") {
    using namespace ne;

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //          this is essential test, some engine systems rely on this
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() = default;
        virtual ~MyDerivedNode() override {}

        void start() {
            getGameInstance()->addDeferredTask([this]() { myCallback(); });
        }

    protected:
        std::string sSomePrivateString = "Hello!";
        void myCallback() {
            sSomePrivateString = "It seems to work.";
            getGameInstance()->getWindow()->close();
        }
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override { REQUIRE(bFinished); }
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalWorldError1) {
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                //          this is essential test, some engine systems rely on this
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                const auto iInitialObjectCount = gc_collector()->getAliveObjectsCount();

                auto pMyNode = gc_new<MyDerivedNode>();
                getWorldRootNode()->addChildNode(pMyNode);

                // add deferred task to change world
                createWorld([this, iInitialObjectCount](const std::optional<Error>& optionalWorldError2) {
                    REQUIRE(gc_collector()->getAliveObjectsCount() == iInitialObjectCount);
                    bFinished = true;
                });

                // add deferred task to call our function
                pMyNode->start();

                // engine should finish all deferred tasks before changing the world (before destroying all
                // nodes)
            });
        }

    private:
        bool bFinished = false;
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("use deferred task with node's member function while the garbage collector is running") {
    using namespace ne;

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //          this is essential test, some engine systems rely on this
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    class MyDerivedNode : public Node {
    public:
        MyDerivedNode() = default;
        virtual ~MyDerivedNode() override {}

        void start() {
            getGameInstance()->addDeferredTask([this]() { myCallback(); });
        }

    protected:
        std::string sSomePrivateString = "Hello!";
        void myCallback() {
            sSomePrivateString = "It seems to work.";
            getGameInstance()->getWindow()->close();
        }
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual ~TestGameInstance() override { REQUIRE(bFinished); }
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalWorldError1) {
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                //          this is essential test, some engine systems rely on this
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                const auto iInitialObjectCount = gc_collector()->getAliveObjectsCount();

                // add deferred task to run GC
                queueGarbageCollection(true, [this, iInitialObjectCount]() {
                    REQUIRE(gc_collector()->getAliveObjectsCount() == iInitialObjectCount);
                    bFinished = true;
                });

                {
                    auto pMyNode = gc_new<MyDerivedNode>();

                    // add deferred task to call our function
                    pMyNode->start();
                } // this node is no longer used and can be garbage collected

                // node should be still alive
                REQUIRE(gc_collector()->getAliveObjectsCount() == iInitialObjectCount + 2);

                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                //       engine should finish all deferred tasks before running the GC
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            });
        }

    private:
        bool bFinished = false;
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("detach and despawn spawned node") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>&) {
                REQUIRE(getTotalSpawnedNodeCount() == 0); // world root node is still in deferred task

                pMyNode = gc_new<Node>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            iTickCount += 1;

            if (iTickCount == 1) {
                REQUIRE(getTotalSpawnedNodeCount() == 2);

                pMyNode->detachFromParentAndDespawn();
                pMyNode = nullptr;
                queueGarbageCollection(true);
            } else {
                REQUIRE(getTotalSpawnedNodeCount() == 1);
                REQUIRE(Node::getAliveNodeCount() == 1);

                getWindow()->close();
            }
        }
        virtual ~TestGameInstance() override {}

    private:
        size_t iTickCount = 0;
        gc<Node> pMyNode;
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("input event callbacks and tick in Node is not triggered after despawning") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() {
            setReceiveInput(true);
            setIsCalledEveryFrame(true);

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
        }

        bool bAction1Triggered = false;
        bool bAxis1Triggered = false;
        size_t iTickCalledCount = 0;

    protected:
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            Node::onBeforeNewFrame(fTimeSincePrevCallInSec);

            iTickCalledCount += 1;
        }

    private:
        void action1(KeyboardModifiers modifiers, bool bIsPressedDown) { bAction1Triggered = true; }
        void axis1(KeyboardModifiers modifiers, float input) { bAxis1Triggered = true; }
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world

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
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            iTickCount += 1;

            if (iTickCount == 1) {
                // Simulate input.
                getWindow()->onKeyboardInput(KeyboardKey::KEY_A, KeyboardModifiers(0), true);
                getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);

                REQUIRE(pMyNode->bAction1Triggered);
                REQUIRE(pMyNode->bAxis1Triggered);

                REQUIRE(getTotalSpawnedNodeCount() == 2);

                // GameInstance is ticking before nodes.
                REQUIRE(pMyNode->iTickCalledCount == 0);
            } else if (iTickCount == 2) {
                REQUIRE(pMyNode->iTickCalledCount == 1);

                pMyNode->detachFromParentAndDespawn();

                REQUIRE(getTotalSpawnedNodeCount() == 2); // still in world
            } else if (iTickCount == 3) {
                // Node was called in previous tick (because not despawned instantly), should no longer tick.
                REQUIRE(pMyNode->iTickCalledCount == 2);

                REQUIRE(getTotalSpawnedNodeCount() == 1); // removed from world
            } else if (iTickCount == 4) {
                REQUIRE(pMyNode->iTickCalledCount == 2); // no longer ticking

                pMyNode = nullptr;

                queueGarbageCollection(true);
            } else {
                REQUIRE(getTotalSpawnedNodeCount() == 1);
                REQUIRE(Node::getAliveNodeCount() == 1);

                getWindow()->close();
            }
        }
        virtual ~TestGameInstance() override {}

    private:
        size_t iTickCount = 0;
        gc<MyNode> pMyNode;
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