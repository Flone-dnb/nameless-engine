// Custom.
#include "game/nodes/Node.h"
#include "game/GameInstance.h"
#include "game/Window.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("node names should not be unique") {
    using namespace ne;

    const auto sNodeName = "Test Node Name";

    const auto pNode1 = gc_new<Node>(sNodeName);
    const auto pNode2 = gc_new<Node>(sNodeName);

    REQUIRE(pNode1->getName() == sNodeName);
    REQUIRE(pNode2->getName() == sNodeName);
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

        // Check that everything is correct.
        REQUIRE(pParentNode->getChildNodes()->size() == 1);
        REQUIRE(&*pParentNode->getChildNodes()->operator[](0) == &*pChildNode);

        REQUIRE(pChildNode->getChildNodes()->size() == 2);
        REQUIRE(&*pChildNode->getChildNodes()->operator[](0) == &*pChildChildNode1);
        REQUIRE(&*pChildNode->getChildNodes()->operator[](1) == &*pChildChildNode2);

        REQUIRE(pChildNode->getParentNode() == pParentNode);
        REQUIRE(pChildChildNode1->getParentNode() == pChildNode);
        REQUIRE(pChildChildNode2->getParentNode() == pChildNode);

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

        const auto pCharacterChildNode1 = gc_new<Node>();
        const auto pCharacterChildNode2 = gc_new<Node>();

        // Build hierarchy.
        pCharacterNode->addChildNode(pCharacterChildNode1);
        pCharacterNode->addChildNode(pCharacterChildNode2);
        pParentNode->addChildNode(pCharacterNode);
        pParentNode->addChildNode(pCarNode);

        // Attach the character to the car.
        pCarNode->addChildNode(pCharacterNode);

        // Check that everything is correct.
        REQUIRE(&*pCharacterNode->getParentNode() == &*pCarNode);
        REQUIRE(pCharacterNode->getChildNodes()->size() == 2);
        REQUIRE(pCharacterChildNode1->isChildOf(&*pCharacterNode));
        REQUIRE(pCharacterChildNode2->isChildOf(&*pCharacterNode));

        // Detach the character from the car.
        pParentNode->addChildNode(pCharacterNode);

        // Check that everything is correct.
        REQUIRE(&*pCharacterNode->getParentNode() == &*pParentNode);
        REQUIRE(pCharacterNode->getChildNodes()->size() == 2);
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
    const std::filesystem::path pathToFile = std::filesystem::temp_directory_path() /
                                             "TESTING_NodeTree_TESTING"; // not specifying ".toml" on purpose
    const std::filesystem::path fullPathToFile =
        std::filesystem::temp_directory_path() / "TESTING_NodeTree_TESTING.toml";

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
            INFO(err.getError());
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
            INFO(err.getError());
            REQUIRE(false);
        }
        const auto pRootNode = std::get<gc<Node>>(deserializeResult);

        // Check results.
        REQUIRE(pRootNode->getName() == "Root Node");
        const auto vChildNodes = pRootNode->getChildNodes();
        REQUIRE(vChildNodes->size() == 2);

        // Check child nodes.
        gc<Node> pChildNode1;
        gc<Node> pChildNode2;
        if ((*vChildNodes)[0]->getName() == "Child Node 1") {
            REQUIRE((*vChildNodes)[1]->getName() == "Child Node 2");
            pChildNode1 = (*vChildNodes)[0];
            pChildNode2 = (*vChildNodes)[1];
        } else if ((*vChildNodes)[0]->getName() == "Child Node 2") {
            REQUIRE((*vChildNodes)[1]->getName() == "Child Node 1");
            pChildNode1 = (*vChildNodes)[1];
            pChildNode2 = (*vChildNodes)[2];
        }

        // Check for child child nodes.
        REQUIRE(pChildNode2->getChildNodes()->empty());
        const auto vChildChildNodes = pChildNode1->getChildNodes();
        REQUIRE(vChildChildNodes->size() == 1);
        REQUIRE((*vChildChildNodes)[0]->getChildNodes()->empty());
        REQUIRE((*vChildChildNodes)[0]->getName() == "Child Child Node 1");
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
        virtual void onSpawn() override {
            MyDerivedNode::onSpawn();

            bSpawnCalled = true;

            // Get parent without name.
            auto pNode = getParentNodeOfType<MyDerivedNode>();
            REQUIRE(&*pNode == &*getParentNode());
            REQUIRE(pNode->iAnswer == 0);

            // Get parent with name.
            pNode = getParentNodeOfType<MyDerivedNode>("MyDerivedNode");
            REQUIRE(pNode->iAnswer == 42);
        }
        bool bSpawnCalled = false;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld();

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
        virtual void onSpawn() override {
            MyDerivedNode::onSpawn();

            bSpawnCalled = true;

            // Get child without name.
            auto pNode = getChildNodeOfType<MyDerivedNode>();
            REQUIRE(&*pNode == &*getChildNodes()->operator[](0));
            REQUIRE(pNode->iAnswer == 0);

            // Get child with name.
            pNode = getChildNodeOfType<MyDerivedNode>("MyDerivedNode");
            REQUIRE(pNode->iAnswer == 42);
        }
        bool bSpawnCalled = false;
    };

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld();

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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("test GC performance and stability with nodes") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override { createWorld(); }
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
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}