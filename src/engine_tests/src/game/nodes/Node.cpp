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
        pChildNode->addChildNode(
            pChildChildNode1, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
        pChildNode->addChildNode(
            pChildChildNode2, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
        pParentNode->addChildNode(
            pChildNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

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
        pCharacterNode->addChildNode(
            pCharacterChildNode1, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
        pCharacterNode->addChildNode(
            pCharacterChildNode2, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
        pParentNode->addChildNode(
            pCharacterNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
        pParentNode->addChildNode(
            pCarNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

        // Attach the character to the car.
        pCarNode->addChildNode(
            pCharacterNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
        pCarNode->addChildNode(
            pSomeNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

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
        pParentNode->addChildNode(
            pCharacterNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

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
        pRootNode->addChildNode(
            pChildNode1, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
        pRootNode->addChildNode(
            pChildNode2, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
        pChildNode1->addChildNode(
            pChildChildNode1, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

        // Serialize.
        const auto optionalError = pRootNode->serializeNodeTree(pathToFile, false);
        if (optionalError.has_value()) {
            auto err = optionalError.value();
            err.addCurrentLocationToErrorStack();
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
            err.addCurrentLocationToErrorStack();
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create nodes.
                auto pDerivedNodeParent = gc_new<MyDerivedNode>("MyDerivedNode");
                pDerivedNodeParent->iAnswer = 42;

                const auto pDerivedNodeChild = gc_new<MyDerivedNode>();

                auto pDerivedDerivedNode = gc_new<MyDerivedDerivedNode>();

                // Build node hierarchy.
                pDerivedNodeChild->addChildNode(
                    pDerivedDerivedNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pDerivedNodeParent->addChildNode(
                    pDerivedNodeChild,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                getWorldRootNode()->addChildNode(
                    pDerivedNodeParent,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                REQUIRE(pDerivedDerivedNode->bSpawnCalled);

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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                // Create nodes.
                auto pDerivedDerivedNode = gc_new<MyDerivedDerivedNode>();

                auto pDerivedNodeParent = gc_new<MyDerivedNode>();

                const auto pDerivedNodeChild = gc_new<MyDerivedNode>("MyDerivedNode");
                pDerivedNodeChild->iAnswer = 42;

                // Build node hierarchy.
                pDerivedNodeParent->addChildNode(
                    pDerivedNodeChild,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pDerivedDerivedNode->addChildNode(
                    pDerivedNodeParent,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                getWorldRootNode()->addChildNode(
                    pDerivedDerivedNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                REQUIRE(pDerivedDerivedNode->bSpawnCalled);

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
                    error.addCurrentLocationToErrorStack();
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
                getWorldRootNode()->addChildNode(
                    pNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

                // Change world to see if GC will collect everything.
                createWorld([&](const std::optional<Error>& optionalWorldError) {
                    if (optionalWorldError.has_value()) {
                        auto error = optionalWorldError.value();
                        error.addCurrentLocationToErrorStack();
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
        error.addCurrentLocationToErrorStack();
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
            getWorldRootNode()->addChildNode(
                pNewNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
        }
        virtual ~TestGameInstance() override {}

    private:
        void addChildNodes(size_t iChildrenCount, gc<Node> pNode) {
            using namespace ne;

            if (iChildrenCount == 0)
                return;

            const auto pNewNode = gc_new<Node>();
            addChildNodes(iChildrenCount - 1, pNewNode);
            pNode->addChildNode(
                pNewNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
        }
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                REQUIRE(getWorldRootNode());

                REQUIRE(getCalledEveryFrameNodeCount() == 0);

                pNotCalledtNode = gc_new<MyNode>(false);
                getWorldRootNode()->addChildNode(
                    pNotCalledtNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE); // queues deferred task to add to world

                pCalledNode = gc_new<MyNode>(true);
                getWorldRootNode()->addChildNode(
                    pCalledNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE); // queues deferred task to add to world
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
        error.addCurrentLocationToErrorStack();
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                REQUIRE(getWorldRootNode());

                getWorldRootNode()->addChildNode(
                    gc_new<MyFirstNode>(),
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                getWorldRootNode()->addChildNode(
                    gc_new<MySecondNode>(),
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
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
        error.addCurrentLocationToErrorStack();
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
            REQUIRE(isReceivingInput() == false); // disabled by default
            setIsReceivingInput(true);

            {
                const auto pActionEvents = getActionEventBindings();
                std::scoped_lock guard(pActionEvents->first);

                pActionEvents->second[0] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
                    action1(modifiers, bIsPressedDown);
                };
            }

            {
                const auto pAxisEvents = getAxisEventBindings();
                std::scoped_lock guard(pAxisEvents->first);

                pAxisEvents->second[0] = [&](KeyboardModifiers modifiers, float input) {
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(
                    pMyNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE); // queues a deferred task to be added to world

                // Register events.
                auto optionalError = getInputManager()->addActionEvent(0, {KeyboardKey::KEY_W});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                optionalError =
                    getInputManager()->addAxisEvent(0, {{KeyboardKey::KEY_A, KeyboardKey::KEY_B}});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
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
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("use deferred task with node's member function while the world is being changed") {
    using namespace ne;

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //          this is essential test, some engine parts rely on this
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
                //          this is essential test, some engine parts rely on this
                // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

                const auto iInitialObjectCount = gc_collector()->getAliveObjectsCount();

                auto pMyNode = gc_new<MyDerivedNode>();
                getWorldRootNode()->addChildNode(
                    pMyNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

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
        error.addCurrentLocationToErrorStack();
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
    //          this is essential test, some engine parts rely on this
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
                //          this is essential test, some engine parts rely on this
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
        error.addCurrentLocationToErrorStack();
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
                getWorldRootNode()->addChildNode(
                    pMyNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE); // queues a deferred task
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
        error.addCurrentLocationToErrorStack();
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
            setIsReceivingInput(true);
            setIsCalledEveryFrame(true);

            {
                const auto pActionEvents = getActionEventBindings();
                std::scoped_lock guard(pActionEvents->first);

                pActionEvents->second[0] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
                    action1(modifiers, bIsPressedDown);
                };
            }

            {
                const auto pAxisEvents = getAxisEventBindings();
                std::scoped_lock guard(pAxisEvents->first);

                pAxisEvents->second[0] = [&](KeyboardModifiers modifiers, float input) {
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(
                    pMyNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE); // queues a deferred task to be added to world

                // Register events.
                auto optionalError = getInputManager()->addActionEvent(0, {KeyboardKey::KEY_W});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                optionalError =
                    getInputManager()->addAxisEvent(0, {{KeyboardKey::KEY_A, KeyboardKey::KEY_B}});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
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
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("disable \"is called every frame\" in onBeforeNewFrame") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() { setIsCalledEveryFrame(true); }

        size_t iTickCallCount = 0;

    protected:
        virtual void onBeforeNewFrame(float delta) override {
            iTickCallCount += 1;

            setIsCalledEveryFrame(false);
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (pMyNode->iTickCallCount == 1) {
                // Node ticked once and disabled it's ticking, wait a few frames to see that node's tick
                // will not be called.
                bWait = true;
            }

            if (!bWait) {
                return;
            }

            iFramesPassed += 1;
            if (iFramesPassed >= iFramesToWait) {
                REQUIRE(pMyNode->iTickCallCount == 1);
                getWindow()->close();
            }
        }
        virtual ~TestGameInstance() override {}

    private:
        bool bWait = false;
        size_t iFramesPassed = 0;
        const size_t iFramesToWait = 10;
        gc<MyNode> pMyNode;
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

TEST_CASE("disable \"is called every frame\" in onBeforeNewFrame and despawn") {
    using namespace ne;

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    //      this is an important test as it tests a bug that we might have
    //                (bug - node still ticking after being despawned)
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    class MyNode : public Node {
    public:
        MyNode() { setIsCalledEveryFrame(true); }

        size_t iTickCallCount = 0;

    protected:
        virtual void onBeforeNewFrame(float delta) override {
            iTickCallCount += 1;

            setIsCalledEveryFrame(false);

            detachFromParentAndDespawn();
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (pMyNode->iTickCallCount == 1) {
                // Node ticked once and disabled it's ticking, wait a few frames to see that node's tick
                // will not be called.
                bWait = true;
            }

            if (!bWait) {
                return;
            }

            iFramesPassed += 1;
            if (iFramesPassed >= iFramesToWait) {
                REQUIRE(pMyNode->iTickCallCount == 1);
                getWindow()->close();
            }
        }
        virtual ~TestGameInstance() override {}

    private:
        bool bWait = false;
        size_t iFramesPassed = 0;
        const size_t iFramesToWait = 10;
        gc<MyNode> pMyNode;
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

TEST_CASE("quickly enable and disable \"is called every frame\" while spawned") {
    using namespace ne;

    class MyNode : public Node {
    public:
        size_t iTickCallCount = 0;

        void test() {
            REQUIRE(isCalledEveryFrame() == false);
            setIsCalledEveryFrame(true);
            setIsCalledEveryFrame(false);
        }

    protected:
        virtual void onBeforeNewFrame(float delta) override {
            REQUIRE(false);
            iTickCallCount += 1;
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (!bWait) {
                pMyNode->test();
                bWait = true;
                REQUIRE(pMyNode->iTickCallCount == 0);
                return;
            }

            iFramesPassed += 1;
            if (iFramesPassed >= iFramesToWait) {
                REQUIRE(pMyNode->iTickCallCount == 0);
                getWindow()->close();
            }
        }
        virtual ~TestGameInstance() override {}

    private:
        bool bWait = false;
        size_t iFramesPassed = 0;
        const size_t iFramesToWait = 10;
        gc<MyNode> pMyNode;
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

TEST_CASE("quickly enable, disable and enable \"is called every frame\" while spawned") {
    using namespace ne;

    class MyNode : public Node {
    public:
        size_t iTickCallCount = 0;

        void test() {
            REQUIRE(isCalledEveryFrame() == false);
            setIsCalledEveryFrame(true);
            setIsCalledEveryFrame(false);
            setIsCalledEveryFrame(true);
        }

    protected:
        virtual void onBeforeNewFrame(float delta) override { iTickCallCount += 1; }
    };

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

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (!bWait) {
                pMyNode->test();
                bWait = true;
                REQUIRE(pMyNode->iTickCallCount == 0);
                return;
            }

            iFramesPassed += 1;
            if (iFramesPassed >= iFramesToWait) {
                REQUIRE(pMyNode->iTickCallCount > 0);
                REQUIRE(pMyNode->isCalledEveryFrame());
                getWindow()->close();
            }
        }
        virtual ~TestGameInstance() override {}

    private:
        bool bWait = false;
        size_t iFramesPassed = 0;
        const size_t iFramesToWait = 10;
        gc<MyNode> pMyNode;
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

TEST_CASE("enable \"is called every frame\" while spawned and despawn") {
    using namespace ne;

    class MyNode : public Node {
    public:
        size_t iTickCallCount = 0;

        void test() {
            REQUIRE(isCalledEveryFrame() == false);
            setIsCalledEveryFrame(true);
            detachFromParentAndDespawn();
        }

    protected:
        virtual void onBeforeNewFrame(float delta) override {
            REQUIRE(false);
            iTickCallCount += 1;
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (!bWait) {
                pMyNode->test();
                bWait = true;
                REQUIRE(pMyNode->iTickCallCount == 0);
                return;
            }

            iFramesPassed += 1;
            if (iFramesPassed >= iFramesToWait) {
                REQUIRE(pMyNode->iTickCallCount == 0);
                REQUIRE(pMyNode->isCalledEveryFrame());
                getWindow()->close();
            }
        }
        virtual ~TestGameInstance() override {}

    private:
        bool bWait = false;
        size_t iFramesPassed = 0;
        const size_t iFramesToWait = 10;
        gc<MyNode> pMyNode;
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

TEST_CASE("enable \"is called every frame\" after despawn") {
    using namespace ne;

    class MyNode : public Node {
    public:
        size_t iTickCallCount = 0;

        void test() {
            REQUIRE(isCalledEveryFrame() == false);
            detachFromParentAndDespawn();
            setIsCalledEveryFrame(true);
        }

    protected:
        virtual void onBeforeNewFrame(float delta) override {
            REQUIRE(false);
            iTickCallCount += 1;
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (!bWait) {
                pMyNode->test();
                bWait = true;
                REQUIRE(pMyNode->iTickCallCount == 0);
                return;
            }

            iFramesPassed += 1;
            if (iFramesPassed >= iFramesToWait) {
                REQUIRE(pMyNode->iTickCallCount == 0);
                REQUIRE(pMyNode->isCalledEveryFrame());
                getWindow()->close();
            }
        }
        virtual ~TestGameInstance() override {}

    private:
        bool bWait = false;
        size_t iFramesPassed = 0;
        const size_t iFramesToWait = 10;
        gc<MyNode> pMyNode;
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

TEST_CASE("disable receiving input while processing input") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() {
            REQUIRE(isReceivingInput() == false); // disabled by default
            setIsReceivingInput(true);

            {
                const auto pActionEvents = getActionEventBindings();
                std::scoped_lock guard(pActionEvents->first);

                pActionEvents->second[0] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
                    action1(modifiers, bIsPressedDown);
                };
            }
        }

        size_t iAction1TriggerCount = 0;

    private:
        void action1(KeyboardModifiers modifiers, bool bIsPressedDown) {
            iAction1TriggerCount += 1;

            setIsReceivingInput(false);
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world

                // Register event.
                auto optionalError = getInputManager()->addActionEvent(0, {KeyboardKey::KEY_W});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (!bInitialTriggerFinished) {
                // Simulate input.
                getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);
                REQUIRE(pMyNode->iAction1TriggerCount == 1);

                // node should disable its input processing now using a deferred task
                // wait 1 frame
                bInitialTriggerFinished = true;
                return;
            }

            // Simulate input again.
            getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), false);
            REQUIRE(pMyNode->iAction1TriggerCount == 1);

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}

    private:
        gc<MyNode> pMyNode;
        bool bInitialTriggerFinished = false;
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

TEST_CASE("disable receiving input and despawn") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() {
            REQUIRE(isReceivingInput() == false); // disabled by default
            setIsReceivingInput(true);

            {
                const auto pActionEvents = getActionEventBindings();
                std::scoped_lock guard(pActionEvents->first);

                pActionEvents->second[0] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
                    action1(modifiers, bIsPressedDown);
                };
            }
        }

        void test() {
            setIsReceivingInput(false);
            detachFromParentAndDespawn();
        }

        size_t iAction1TriggerCount = 0;

    private:
        void action1(KeyboardModifiers modifiers, bool bIsPressedDown) { iAction1TriggerCount += 1; }
    };

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

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world

                // Register event.
                auto optionalError = getInputManager()->addActionEvent(0, {KeyboardKey::KEY_W});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (!bInitialTriggerFinished) {
                // Simulate input.
                getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);
                REQUIRE(pMyNode->iAction1TriggerCount == 1);

                pMyNode->test();

                // node should disable its input processing now using a deferred task
                // wait 1 frame

                bInitialTriggerFinished = true;
                return;
            }

            // Simulate input again.
            getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), false);
            REQUIRE(pMyNode->iAction1TriggerCount == 1);

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}

    private:
        gc<MyNode> pMyNode;
        bool bInitialTriggerFinished = false;
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

TEST_CASE("enable receiving input and despawn") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() {
            REQUIRE(isReceivingInput() == false); // disabled by default

            {
                const auto pActionEvents = getActionEventBindings();
                std::scoped_lock guard(pActionEvents->first);

                pActionEvents->second[0] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
                    action1(modifiers, bIsPressedDown);
                };
            }
        }

        void test() {
            setIsReceivingInput(true);
            detachFromParentAndDespawn();
        }

        size_t iAction1TriggerCount = 0;

    private:
        void action1(KeyboardModifiers modifiers, bool bIsPressedDown) { iAction1TriggerCount += 1; }
    };

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

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world

                // Register event.
                auto optionalError = getInputManager()->addActionEvent(0, {KeyboardKey::KEY_W});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (!bInitialTriggerFinished) {
                // Simulate input.
                getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);
                REQUIRE(pMyNode->iAction1TriggerCount == 0);

                pMyNode->test();

                // wait 1 frame
                bInitialTriggerFinished = true;
                return;
            }

            // Simulate input again.
            getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), false);
            REQUIRE(pMyNode->iAction1TriggerCount == 0);

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}

    private:
        gc<MyNode> pMyNode;
        bool bInitialTriggerFinished = false;
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

TEST_CASE("enable receiving input while spawned") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() {
            REQUIRE(isReceivingInput() == false); // disabled by default

            {
                const auto pActionEvents = getActionEventBindings();
                std::scoped_lock guard(pActionEvents->first);

                pActionEvents->second[0] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
                    action1(modifiers, bIsPressedDown);
                };
            }
        }

        void test() {
            REQUIRE(isReceivingInput() == false);
            setIsReceivingInput(true);
        }

        size_t iAction1TriggerCount = 0;

    private:
        void action1(KeyboardModifiers modifiers, bool bIsPressedDown) { iAction1TriggerCount += 1; }
    };

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

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world

                // Register event.
                auto optionalError = getInputManager()->addActionEvent(0, {KeyboardKey::KEY_W});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (!bInitialTriggerFinished) {
                // Simulate input.
                getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);
                REQUIRE(pMyNode->iAction1TriggerCount == 0);

                pMyNode->test();

                // wait 1 frame
                bInitialTriggerFinished = true;
                return;
            }

            // Simulate input again.
            getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), false);
            REQUIRE(pMyNode->iAction1TriggerCount == 1);

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}

    private:
        gc<MyNode> pMyNode;
        bool bInitialTriggerFinished = false;
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

TEST_CASE("quickly enable receiving input and disable while spawned") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() {
            REQUIRE(isReceivingInput() == false); // disabled by default

            {
                const auto pActionEvents = getActionEventBindings();
                std::scoped_lock guard(pActionEvents->first);

                pActionEvents->second[0] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
                    action1(modifiers, bIsPressedDown);
                };
            }
        }

        void test() {
            REQUIRE(isReceivingInput() == false);
            setIsReceivingInput(true);
            setIsReceivingInput(false);
        }

        size_t iAction1TriggerCount = 0;

    private:
        void action1(KeyboardModifiers modifiers, bool bIsPressedDown) { iAction1TriggerCount += 1; }
    };

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

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world

                // Register event.
                auto optionalError = getInputManager()->addActionEvent(0, {KeyboardKey::KEY_W});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (!bInitialTriggerFinished) {
                // Simulate input.
                getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);
                REQUIRE(pMyNode->iAction1TriggerCount == 0);

                pMyNode->test();

                // wait 1 frame
                bInitialTriggerFinished = true;
                return;
            }

            // Simulate input again.
            getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), false);
            REQUIRE(pMyNode->iAction1TriggerCount == 0);

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}

    private:
        gc<MyNode> pMyNode;
        bool bInitialTriggerFinished = false;
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

TEST_CASE("quickly disable receiving input and enable while spawned") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() {
            REQUIRE(isReceivingInput() == false); // disabled by default
            setIsReceivingInput(true);

            {
                const auto pActionEvents = getActionEventBindings();
                std::scoped_lock guard(pActionEvents->first);

                pActionEvents->second[0] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
                    action1(modifiers, bIsPressedDown);
                };
            }
        }

        void test() {
            REQUIRE(isReceivingInput() == true);
            setIsReceivingInput(false);
            setIsReceivingInput(true);
        }

        size_t iAction1TriggerCount = 0;

    private:
        void action1(KeyboardModifiers modifiers, bool bIsPressedDown) { iAction1TriggerCount += 1; }
    };

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

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world

                // Register event.
                auto optionalError = getInputManager()->addActionEvent(0, {KeyboardKey::KEY_W});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
            });
        }
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            if (!bInitialTriggerFinished) {
                // Simulate input.
                getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);
                REQUIRE(pMyNode->iAction1TriggerCount == 1);

                pMyNode->test();

                // wait 1 frame
                bInitialTriggerFinished = true;
                return;
            }

            // Simulate input again.
            getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), false);
            REQUIRE(pMyNode->iAction1TriggerCount == 2);

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}

    private:
        gc<MyNode> pMyNode;
        bool bInitialTriggerFinished = false;
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

TEST_CASE("input event callbacks are only triggered when input changed") {
    using namespace ne;

    class MyNode : public Node {
    public:
        MyNode() {
            setIsReceivingInput(true);

            {
                const auto pActionEvents = getActionEventBindings();
                std::scoped_lock guard(pActionEvents->first);

                pActionEvents->second[0] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
                    action1(modifiers, bIsPressedDown);
                };
            }

            {
                const auto pAxisEvents = getAxisEventBindings();
                std::scoped_lock guard(pAxisEvents->first);

                pAxisEvents->second[0] = [&](KeyboardModifiers modifiers, float input) {
                    axis1(modifiers, input);
                };
            }
        }

        size_t iAction1TriggerCount = 0;
        size_t iAxis1TriggerCount = 0;

    private:
        void action1(KeyboardModifiers modifiers, bool bIsPressedDown) { iAction1TriggerCount += 1; }
        void axis1(KeyboardModifiers modifiers, float input) { iAxis1TriggerCount += 1; }
    };

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

                // Spawn node.
                pMyNode = gc_new<MyNode>();
                getWorldRootNode()->addChildNode(pMyNode); // queues a deferred task to be added to world

                // Register events.
                auto optionalError = getInputManager()->addActionEvent(0, {KeyboardKey::KEY_W});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                optionalError =
                    getInputManager()->addAxisEvent(0, {{KeyboardKey::KEY_A, KeyboardKey::KEY_D}});
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
            });
        }

        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) override {
            // Simulate "pressed" input.
            getWindow()->onKeyboardInput(KeyboardKey::KEY_A, KeyboardModifiers(0), true);
            getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);

            REQUIRE(pMyNode->iAction1TriggerCount == 1);
            REQUIRE(pMyNode->iAxis1TriggerCount == 1);

            // Simulate the same "pressed" input again.
            getWindow()->onKeyboardInput(KeyboardKey::KEY_A, KeyboardModifiers(0), true);
            getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), true);

            // Input callbacks should not be triggered since the input is the same as the last one.
            REQUIRE(pMyNode->iAction1TriggerCount == 1);
            REQUIRE(pMyNode->iAxis1TriggerCount == 1);

            getWindow()->onKeyboardInput(KeyboardKey::KEY_A, KeyboardModifiers(0), false);
            getWindow()->onKeyboardInput(KeyboardKey::KEY_W, KeyboardModifiers(0), false);

            // Input differs from the last one.
            REQUIRE(pMyNode->iAction1TriggerCount == 2);
            REQUIRE(pMyNode->iAxis1TriggerCount == 2);

            getWindow()->close();
        }

        virtual ~TestGameInstance() override {}

    private:
        gc<MyNode> pMyNode;
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

TEST_CASE("serialize node tree while some nodes marked as not serialize") {
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

                // Prepare path.
                const std::filesystem::path pathToFile =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                    "TESTING_MixedNodeTree_TESTING.toml";

                {
                    // Prepare serialized nodes.
                    const auto pParentNode = gc_new<Node>("serialized parent node");
                    const auto pChildNode = gc_new<Node>("serialized child node");
                    pParentNode->addChildNode(pChildNode);

                    // Spawn serialized nodes.
                    getWorldRootNode()->addChildNode(pParentNode);
                }

                {
                    // Prepare nodes that won't be serialized.
                    const auto pParentNode = gc_new<Node>("not serialized parent node");
                    pParentNode->setSerialize(false);

                    const auto pChildNode = gc_new<Node>("not serialized child node");
                    pChildNode->setSerialize(true); // explicitly mark to be serialized to make sure it won't
                                                    // be serialized because parent is not serialized

                    pParentNode->addChildNode(pChildNode);

                    // Spawn non-serialized nodes.
                    getWorldRootNode()->addChildNode(pParentNode);
                }

                {
                    // Serialize node tree.
                    auto optionalError = getWorldRootNode()->serializeNodeTree(pathToFile, false);
                    if (optionalError.has_value()) [[unlikely]] {
                        optionalError->addCurrentLocationToErrorStack();
                        INFO(optionalError->getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                {
                    // Deserialize node tree.
                    auto result = Node::deserializeNodeTree(pathToFile);
                    if (std::holds_alternative<Error>(result)) [[unlikely]] {
                        auto error = std::get<Error>(std::move(result));
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    const auto pRootNode = std::get<gc<Node>>(std::move(result));

                    // Get root's child nodes.
                    const auto pMtxRootChildNodes = pRootNode->getChildNodes();
                    std::scoped_lock rootChildNodesGuard(pMtxRootChildNodes->first);

                    // Make sure we have only 1 child node under root.
                    REQUIRE(pMtxRootChildNodes->second->size() == 1);
                    const auto pParentNode = pMtxRootChildNodes->second->at(0);

                    // Make sure the name is correct.
                    REQUIRE(pParentNode->getNodeName() == "serialized parent node");

                    // Get parent's child nodes.
                    const auto pMtxParentChildNodes = pParentNode->getChildNodes();
                    std::scoped_lock parentChildNodesGuard(pMtxParentChildNodes->first);

                    // Make sure we have only 1 child node under root.
                    REQUIRE(pMtxParentChildNodes->second->size() == 1);
                    const auto pChildNode = pMtxParentChildNodes->second->at(0);

                    // Make sure the name is correct.
                    REQUIRE(pChildNode->getNodeName() == "serialized child node");
                }

                getWindow()->close();
            });
        }
        virtual ~TestGameInstance() override {}

    private:
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
