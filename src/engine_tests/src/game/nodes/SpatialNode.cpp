// Custom.
#include "game/nodes/SpatialNode.h"
#include "game/GameInstance.h"
#include "math/MathHelpers.hpp"
#include "game/Window.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("world location, rotation and scale are calculated correctly (no parent)") {

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

                constexpr float floatEpsilon = 0.00001f;

                const auto targetWorldLocation = glm::vec3(1.0f, 2.0f, 3.0f);
                const auto targetWorldRotation = glm::vec3(10.0f, 20.0f, 30.0f);
                const auto targetWorldScale = glm::vec3(5.0f, 6.0f, 7.0f);

                const auto pSpatialNode = sgc::makeGc<SpatialNode>("My Cool Spatial Node");

                pSpatialNode->setRelativeLocation(targetWorldLocation);
                pSpatialNode->setRelativeRotation(targetWorldRotation);
                pSpatialNode->setRelativeScale(targetWorldScale);

                const auto worldLocation = pSpatialNode->getWorldLocation();
                const auto worldRotation = pSpatialNode->getWorldRotation();
                const auto worldScale = pSpatialNode->getWorldScale();

                REQUIRE(glm::all(glm::epsilonEqual(worldLocation, targetWorldLocation, floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(worldRotation, targetWorldRotation, floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(worldScale, targetWorldScale, floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("world location is calculated correctly when rotating parent by X") {
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

                constexpr float floatEpsilon = 0.00001f;

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pParentSpatialNode->setRelativeRotation(glm::vec3(-90.0f, 0.0f, 0.0f));

                const auto pChildSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->addChildNode(
                    pChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildSpatialNode->getWorldLocation(), glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));

                pChildSpatialNode->setRelativeLocation(glm::vec3(0.0f, 5.0f, 0.0f));

                const auto worldLocation = pChildSpatialNode->getWorldLocation();

                REQUIRE(
                    glm::all(glm::epsilonEqual(worldLocation, glm::vec3(5.0f, 0.0f, -5.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("world location is calculated correctly when rotating parent by Y") {
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

                constexpr float floatEpsilon = 0.00001f;

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeLocation(glm::vec3(0.0f, 5.0f, 0.0f));
                pParentSpatialNode->setRelativeRotation(glm::vec3(0.0f, -90.0f, 0.0f));

                const auto pChildSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->addChildNode(
                    pChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildSpatialNode->getWorldLocation(), glm::vec3(0.0f, 5.0f, 0.0f), floatEpsilon)));

                pChildSpatialNode->setRelativeLocation(glm::vec3(0.0f, 0.0f, 5.0f));

                const auto worldLocation = pChildSpatialNode->getWorldLocation();

                REQUIRE(
                    glm::all(glm::epsilonEqual(worldLocation, glm::vec3(-5.0f, 5.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("world location is calculated correctly when rotating parent by Z") {
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

                constexpr float floatEpsilon = 0.00001f;

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeLocation(glm::vec3(0.0f, 0.0f, 5.0f));
                pParentSpatialNode->setRelativeRotation(glm::vec3(0.0f, 0.0f, -90.0f));

                const auto pChildSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->addChildNode(
                    pChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildSpatialNode->getWorldLocation(), glm::vec3(0.0f, 0.0f, 5.0f), floatEpsilon)));

                pChildSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));

                const auto worldLocation = pChildSpatialNode->getWorldLocation();

                REQUIRE(
                    glm::all(glm::epsilonEqual(worldLocation, glm::vec3(0.0f, -5.0f, 5.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("world location, rotation and scale are calculated correctly (with parent)") {
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

                constexpr float floatEpsilon = 0.00001f;

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>("My Cool Parent Spatial Node");
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pParentSpatialNode->setRelativeScale(glm::vec3(5.0f, 1.0f, 1.0f));

                const auto pChildSpatialNode = sgc::makeGc<SpatialNode>("My Cool Child Spatial Node");
                pParentSpatialNode->addChildNode(
                    pChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildSpatialNode->getWorldLocation(), glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));

                pChildSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pChildSpatialNode->setRelativeScale(glm::vec3(1.0f, 1.0f, 5.0f));

                const auto worldLocation = pChildSpatialNode->getWorldLocation();
                const auto worldRotation = pChildSpatialNode->getWorldRotation();
                const auto worldScale = pChildSpatialNode->getWorldScale();

                REQUIRE(
                    glm::all(glm::epsilonEqual(worldLocation, glm::vec3(30.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(worldRotation, glm::vec3(0.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(worldScale, glm::vec3(5.0f, 1.0f, 5.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("move parent node with rotation") {
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

                constexpr float floatEpsilon = 0.001f;

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeRotation(glm::vec3(0.0F, 0.0F, 90.0F));

                const auto pChildSpatialNode = sgc::makeGc<SpatialNode>();

                // Spawn in world.
                pParentSpatialNode->addChildNode(
                    pChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                getWorldRootNode()->addChildNode(
                    pParentSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                // Set relative location.
                pChildSpatialNode->setRelativeLocation(glm::vec3(10.0F, 0.0F, 0.0F));

                auto childWorldLocation = pChildSpatialNode->getWorldLocation();
                auto childRelativeLocation = pChildSpatialNode->getRelativeLocation();

                // Check.
                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldLocation, glm::vec3(0.0F, 10.0F, 0.0F), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childRelativeLocation, glm::vec3(10.0F, 0.0F, 0.0F), floatEpsilon)));

                // Move parent.
                pParentSpatialNode->setRelativeLocation(glm::vec3(0.0F, 5.0F, 0.0F));

                childWorldLocation = pChildSpatialNode->getWorldLocation();
                childRelativeLocation = pChildSpatialNode->getRelativeLocation();

                // Check.
                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldLocation, glm::vec3(0.0F, 15.0F, 0.0F), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childRelativeLocation, glm::vec3(10.0F, 0.0F, 0.0F), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE(
    "world location, rotation and scale are calculated correctly (with non spatial nodes in the hierarchy)") {
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

                constexpr float floatEpsilon = 0.00001f;

                // Create nodes.
                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>("My Cool Parent Spatial Node");
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pParentSpatialNode->setRelativeScale(glm::vec3(5.0f, 1.0f, 1.0f));

                const auto pUsualNode1 = sgc::makeGc<Node>("Usual Node 1");

                const auto pSpatialNode = sgc::makeGc<SpatialNode>("My Cool Child Spatial Node");

                const auto pUsualNode2 = sgc::makeGc<Node>("Usual Node 2");

                const auto pChildSpatialNode = sgc::makeGc<SpatialNode>("My Cool Child Spatial Node 1");
                pChildSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pChildSpatialNode->setRelativeScale(glm::vec3(1.0f, 1.0f, 5.0f));

                const auto pUsualNode3 = sgc::makeGc<Node>("Usual Node 3");

                const auto pChildChildSpatialNode = sgc::makeGc<SpatialNode>("My Cool Child Spatial Node 2");
                pChildChildSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pChildChildSpatialNode->setRelativeScale(glm::vec3(1.0f, 1.0f, 5.0f));

                // Build hierarchy.
                pParentSpatialNode->addChildNode(
                    pUsualNode1,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pUsualNode1->addChildNode(
                    pSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pSpatialNode->addChildNode(
                    pUsualNode2,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pUsualNode2->addChildNode(
                    pChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pChildSpatialNode->addChildNode(
                    pUsualNode3,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pUsualNode3->addChildNode(
                    pChildChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                // Check locations.
                const auto worldLocation = pChildChildSpatialNode->getWorldLocation();
                const auto worldRotation = pChildChildSpatialNode->getWorldRotation();
                const auto worldScale = pChildChildSpatialNode->getWorldScale();

                REQUIRE(
                    glm::all(glm::epsilonEqual(worldLocation, glm::vec3(55.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(worldRotation, glm::vec3(0.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(worldScale, glm::vec3(5.0f, 1.0f, 25.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("world location with parent rotation is correct") {
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

                float floatEpsilon = 0.00001f;

                // Create nodes.
                const auto pParentSpatialNodeA = sgc::makeGc<SpatialNode>();
                pParentSpatialNodeA->setRelativeRotation(glm::vec3(0.0f, 0.0f, 45.0f));

                const auto pParentSpatialNodeB = sgc::makeGc<SpatialNode>();
                pParentSpatialNodeB->setRelativeRotation(glm::vec3(90.0f, 0.0f, 0.0f));

                const auto pSpatialNodeA = sgc::makeGc<SpatialNode>();
                pSpatialNodeA->setRelativeRotation(glm::vec3(0.0f, 0.0f, 45.0f));
                pSpatialNodeA->setRelativeLocation(glm::vec3(10.0f, 0.0f, 0.0f));

                const auto pSpatialNodeB = sgc::makeGc<SpatialNode>();
                pSpatialNodeB->setRelativeRotation(glm::vec3(0.0f, 0.0f, 45.0f));

                const auto pSpatialNodeC = sgc::makeGc<SpatialNode>();
                pSpatialNodeC->setRelativeRotation(glm::vec3(0.0f, 0.0f, 90.0f));
                pSpatialNodeC->setRelativeLocation(glm::vec3(0.0F, 5.0F, 0.0F));

                const auto pChildSpatialNodeA = sgc::makeGc<SpatialNode>();
                const auto pChildSpatialNodeB = sgc::makeGc<SpatialNode>();
                pChildSpatialNodeB->setRelativeLocation(glm::vec3(10.0f, 0.0f, 0.0f));

                const auto pChildSpatialNodeC = sgc::makeGc<SpatialNode>();
                pChildSpatialNodeC->setRelativeLocation(glm::vec3(0.0f, 10.0f, 0.0f));

                // Build hierarchy.
                pParentSpatialNodeA->addChildNode(
                    pSpatialNodeA,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pParentSpatialNodeA->addChildNode(
                    pSpatialNodeB,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pParentSpatialNodeB->addChildNode(
                    pSpatialNodeC,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pSpatialNodeA->addChildNode(
                    pChildSpatialNodeA,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pSpatialNodeB->addChildNode(
                    pChildSpatialNodeB,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pSpatialNodeC->addChildNode(
                    pChildSpatialNodeC,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                getWorldRootNode()->addChildNode(
                    pParentSpatialNodeA,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                getWorldRootNode()->addChildNode(
                    pParentSpatialNodeB,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                // Check location.
                const auto middleANodeWorldLocation = pSpatialNodeA->getWorldLocation();
                const auto childANodeWorldLocation = pChildSpatialNodeA->getWorldLocation();
                const auto childBNodeWorldLocation = pChildSpatialNodeB->getWorldLocation();
                const auto childCNodeWorldLocation = pChildSpatialNodeC->getWorldLocation();
                const auto cNodeWorldLocation = pSpatialNodeC->getWorldLocation();
                REQUIRE(glm::all(
                    glm::epsilonEqual(cNodeWorldLocation, glm::vec3(0.0f, 0.0f, 5.0f), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childCNodeWorldLocation, glm::vec3(-10.0f, 0.0f, 5.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    middleANodeWorldLocation, glm::vec3(7.07106f, 7.07106f, 0.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    childANodeWorldLocation, glm::vec3(7.07106f, 7.07106f, 0.0f), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childBNodeWorldLocation, glm::vec3(0.0f, 10.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("set world location with parent is correct") {
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

                constexpr float floatEpsilon = 0.00001f;

                // Create nodes.
                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>("My Cool Spatial Node");
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 5.0f, 5.0f));
                pParentSpatialNode->setRelativeRotation(glm::vec3(0.0F, 0.0F, 90.0F));

                const auto pUsualNode = sgc::makeGc<Node>("Usual Node");

                const auto pChildSpatialNode = sgc::makeGc<SpatialNode>("My Cool Spatial Node");

                // Build hierarchy.
                getWorldRootNode()->addChildNode(
                    pParentSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pParentSpatialNode->addChildNode(
                    pUsualNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pUsualNode->addChildNode(
                    pChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                // Set world location.
                pChildSpatialNode->setWorldLocation(glm::vec3(-5.0f, -5.0f, -5.0f));

                REQUIRE(glm::all(glm::epsilonEqual(
                    pParentSpatialNode->getWorldLocation(), glm::vec3(5.0f, 5.0f, 5.0f), floatEpsilon)));

                // Check locations.
                const auto childRelativeLocation = pChildSpatialNode->getRelativeLocation();
                const auto childWorldLocation = pChildSpatialNode->getWorldLocation();
                REQUIRE(glm::all(glm::epsilonEqual(
                    childRelativeLocation, glm::vec3(-10.0f, 10.0f, -10.0f), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldLocation, glm::vec3(-5.0f, -5.0f, -5.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("set world rotation with parent is correct") {
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

                float floatEpsilon = 0.001f;

                // Create nodes.
                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>("My Cool Spatial Node");
                pParentSpatialNode->setRelativeRotation(glm::vec3(0.0f, 0.0f, 90.0f));

                const auto pUsualNode = sgc::makeGc<Node>("Usual Node");

                const auto pChildSpatialNode = sgc::makeGc<SpatialNode>("My Cool Spatial Node");

                // Build hierarchy.
                getWorldRootNode()->addChildNode(
                    pParentSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pParentSpatialNode->addChildNode(
                    pUsualNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pUsualNode->addChildNode(
                    pChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                // Set world rotation.
                pChildSpatialNode->setWorldRotation(glm::vec3(0.0f, 0.0f, -90.0f));

                // Check rotations.
                const auto childRelativeRotation = pChildSpatialNode->getRelativeRotation();
                const auto childWorldRotation = pChildSpatialNode->getWorldRotation();
                REQUIRE(glm::all(
                    glm::epsilonEqual(childRelativeRotation, glm::vec3(0.0f, 0.0f, 180.0f), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldRotation, glm::vec3(0.0f, 0.0f, -90.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("set world scale with parent is correct") {
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

                constexpr float floatEpsilon = 0.00001f;

                // Create nodes.
                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>("My Cool Spatial Node");
                pParentSpatialNode->setRelativeScale(glm::vec3(5.0f, 5.0f, 5.0f));

                const auto pUsualNode = sgc::makeGc<Node>("Usual Node");

                const auto pChildSpatialNode = sgc::makeGc<SpatialNode>("My Cool Spatial Node");

                // Build hierarchy.
                getWorldRootNode()->addChildNode(
                    pParentSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pParentSpatialNode->addChildNode(
                    pUsualNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pUsualNode->addChildNode(
                    pChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                // Set world scale.
                pChildSpatialNode->setWorldScale(glm::vec3(2.0f, 2.0f, 2.0f));

                REQUIRE(glm::all(glm::epsilonEqual(
                    pParentSpatialNode->getWorldScale(), glm::vec3(5.0f, 5.0f, 5.0f), floatEpsilon)));

                // Check scale.
                const auto childRelativeScale = pChildSpatialNode->getRelativeScale();
                const auto childWorldScale = pChildSpatialNode->getWorldScale();
                REQUIRE(glm::all(
                    glm::epsilonEqual(childRelativeScale, glm::vec3(0.4f, 0.4f, 0.4f), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(childWorldScale, glm::vec3(2.0f, 2.0f, 2.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("serialize and deserialize SpatialNode") {
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

                const std::filesystem::path pathToFileInTemp =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                    "TESTING_SpatialNodeSerialization_TESTING.toml";

                const auto location = glm::vec3(5123.91827f, -12225.24142f, 3.0f);
                const auto rotation = glm::vec3(-5.0f, 15.0f, -30.0f);
                const auto scale = glm::vec3(10.0f, 20.0f, 30.0f);

                {
                    // Setup.
                    const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                    pSpatialNode->setRelativeLocation(location);
                    pSpatialNode->setRelativeRotation(rotation);
                    pSpatialNode->setRelativeScale(scale);

                    // Serialize.
                    const auto optionalError = pSpatialNode->serialize(pathToFileInTemp, false);
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                {
                    // Deserialize.
                    auto result = Serializable::deserialize<sgc::GcPtr<SpatialNode>>(pathToFileInTemp);
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    const auto pSpatialNode = std::get<sgc::GcPtr<SpatialNode>>(std::move(result));

                    constexpr float floatEpsilon = 0.00001f;

                    const auto relativeLocation = pSpatialNode->getRelativeLocation();
                    const auto relativeRotation = pSpatialNode->getRelativeRotation();
                    const auto relativeScale = pSpatialNode->getRelativeScale();
                    const auto worldLocation = pSpatialNode->getWorldLocation();
                    const auto worldRotation = pSpatialNode->getWorldRotation();
                    const auto worldScale = pSpatialNode->getWorldScale();

                    REQUIRE(glm::all(glm::epsilonEqual(relativeLocation, location, floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(relativeRotation, rotation, floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(relativeScale, scale, floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(worldLocation, location, floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(worldRotation, rotation, floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(worldScale, scale, floatEpsilon)));
                }

                if (std::filesystem::exists(pathToFileInTemp)) {
                    std::filesystem::remove(pathToFileInTemp);
                }

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("make spatial node look at world location with parent rotation") {
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

                float floatEpsilon = 0.001f;

                // Create nodes.
                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                const auto pChildSpatialNode = sgc::makeGc<SpatialNode>();

                // Build hierarchy.
                getWorldRootNode()->addChildNode(
                    pParentSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);
                pParentSpatialNode->addChildNode(
                    pChildSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                // Set parent rotation.
                pParentSpatialNode->setRelativeRotation(glm::vec3(0.0f, 90.0f, 90.0f));

                // Check child forward/right.
                auto childWorldForward = pChildSpatialNode->getWorldForwardDirection();
                auto childWorldRight = pChildSpatialNode->getWorldRightDirection();
                auto childWorldUp = pChildSpatialNode->getWorldUpDirection();

                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldForward, -Globals::WorldDirection::up, floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldRight, -Globals::WorldDirection::forward, floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(childWorldUp, Globals::WorldDirection::right, floatEpsilon)));

                // Set parent rotation.
                pParentSpatialNode->setRelativeRotation(glm::vec3(0.0f, 90.0f, -90.0f));

                // Check child forward/right.
                childWorldForward = pChildSpatialNode->getWorldForwardDirection();
                childWorldRight = pChildSpatialNode->getWorldRightDirection();
                childWorldUp = pChildSpatialNode->getWorldUpDirection();

                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldForward, -Globals::WorldDirection::up, floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldRight, Globals::WorldDirection::forward, floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(childWorldUp, -Globals::WorldDirection::right, floatEpsilon)));

                // Make child node look at +Y.
                auto targetRotation =
                    MathHelpers::convertDirectionToRollPitchYaw(Globals::WorldDirection::right);
                pChildSpatialNode->setWorldRotation(targetRotation);

                // Local forward should look at -X and local right should look at -Z.
                auto relativeRotation = pChildSpatialNode->getRelativeRotation();
                auto relativeForward = MathHelpers::convertRollPitchYawToDirection(relativeRotation);

                // Check child forward/right.
                childWorldForward = pChildSpatialNode->getWorldForwardDirection();
                childWorldRight = pChildSpatialNode->getWorldRightDirection();

                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldForward, Globals::WorldDirection::right, floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldRight, -Globals::WorldDirection::forward, floatEpsilon)));

                // Make child node look at -Y.
                targetRotation = MathHelpers::convertDirectionToRollPitchYaw(-Globals::WorldDirection::right);
                pChildSpatialNode->setWorldRotation(targetRotation);

                // Check child forward/right.
                childWorldForward = pChildSpatialNode->getWorldForwardDirection();
                childWorldRight = pChildSpatialNode->getWorldRightDirection();

                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldForward, -Globals::WorldDirection::right, floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldRight, Globals::WorldDirection::forward, floatEpsilon)));

                // Make child node look at -X.
                targetRotation =
                    MathHelpers::convertDirectionToRollPitchYaw(-Globals::WorldDirection::forward);
                pChildSpatialNode->setWorldRotation(targetRotation);

                // Local forward should look at -Y and local right should look at -Z.
                relativeRotation = pChildSpatialNode->getRelativeRotation();
                relativeForward = MathHelpers::convertRollPitchYawToDirection(relativeRotation);
                REQUIRE(glm::all(
                    glm::epsilonEqual(relativeForward, -Globals::WorldDirection::right, floatEpsilon)));

                // Check child forward/right.
                childWorldForward = pChildSpatialNode->getWorldForwardDirection();
                childWorldRight = pChildSpatialNode->getWorldRightDirection();

                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldForward, -Globals::WorldDirection::forward, floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldRight, -Globals::WorldDirection::right, floatEpsilon)));

                // Make child node look at +Z.
                targetRotation = MathHelpers::convertDirectionToRollPitchYaw(Globals::WorldDirection::up);
                pChildSpatialNode->setWorldRotation(targetRotation);

                // Check child forward/right.
                childWorldForward = pChildSpatialNode->getWorldForwardDirection();
                childWorldRight = pChildSpatialNode->getWorldRightDirection();

                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldForward, Globals::WorldDirection::up, floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(childWorldRight, childWorldRight, floatEpsilon)));

                // Make child node look at -Z.
                targetRotation = MathHelpers::convertDirectionToRollPitchYaw(-Globals::WorldDirection::up);
                pChildSpatialNode->setWorldRotation(targetRotation);

                // Check child forward/right.
                childWorldForward = pChildSpatialNode->getWorldForwardDirection();
                childWorldRight = pChildSpatialNode->getWorldRightDirection();

                REQUIRE(glm::all(
                    glm::epsilonEqual(childWorldForward, -Globals::WorldDirection::up, floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(childWorldRight, childWorldRight, floatEpsilon)));

                // Make child node look at +X+Y.
                auto targetLookDirection = glm::normalize(glm::vec3(1.0F, 1.0F, 0.0F));
                targetRotation = MathHelpers::convertDirectionToRollPitchYaw(targetLookDirection);
                pChildSpatialNode->setWorldRotation(targetRotation);

                // Check child forward.
                childWorldForward = pChildSpatialNode->getWorldForwardDirection();

                REQUIRE(glm::all(glm::epsilonEqual(childWorldForward, targetLookDirection, floatEpsilon)));

                // Make child node look at +Y+Z
                targetLookDirection = glm::normalize(glm::vec3(0.0F, 1.0F, 1.0F));
                targetRotation = MathHelpers::convertDirectionToRollPitchYaw(targetLookDirection);
                pChildSpatialNode->setWorldRotation(targetRotation);

                // Check child forward.
                childWorldForward = pChildSpatialNode->getWorldForwardDirection();

                REQUIRE(glm::all(glm::epsilonEqual(childWorldForward, targetLookDirection, floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("relative location/rotation/scale is considered as world when not spawned (no parent)") {
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

                float floatEpsilon = 0.001f;

                const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                pSpatialNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));
                pSpatialNode->setRelativeRotation(glm::vec3(0.0F, 5.0F, 0.0F));
                pSpatialNode->setRelativeScale(glm::vec3(0.0F, 0.0F, 5.0F));

                REQUIRE(glm::all(glm::epsilonEqual(
                    pSpatialNode->getWorldLocation(), pSpatialNode->getRelativeLocation(), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pSpatialNode->getWorldRotation(), pSpatialNode->getRelativeRotation(), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pSpatialNode->getWorldScale(), pSpatialNode->getRelativeScale(), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("use `reset` attachment rule for location") {
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

                float floatEpsilon = 0.001f;

                const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                pSpatialNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(
                    pSpatialNode, Node::AttachmentRule::RESET_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

                const auto relativeLocation = pSpatialNode->getRelativeLocation();
                const auto worldLocation = pSpatialNode->getWorldLocation();
                REQUIRE(
                    glm::all(glm::epsilonEqual(relativeLocation, glm::vec3(0.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(worldLocation, glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("use `keepRelative` attachment rule for location") {
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

                float floatEpsilon = 0.001f;

                const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                pSpatialNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(
                    pSpatialNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

                const auto relativeLocation = pSpatialNode->getRelativeLocation();
                const auto worldLocation = pSpatialNode->getWorldLocation();
                REQUIRE(
                    glm::all(glm::epsilonEqual(relativeLocation, glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(worldLocation, glm::vec3(10.0f, 0.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("use `keepWorld` attachment rule for location") {
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

                float floatEpsilon = 0.001f;

                const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                pSpatialNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(
                    pSpatialNode, Node::AttachmentRule::KEEP_WORLD, Node::AttachmentRule::KEEP_RELATIVE);

                const auto relativeLocation = pSpatialNode->getRelativeLocation();
                const auto worldLocation = pSpatialNode->getWorldLocation();
                REQUIRE(
                    glm::all(glm::epsilonEqual(relativeLocation, glm::vec3(0.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(worldLocation, glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("use `reset` attachment rule for rotation") {
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

                float floatEpsilon = 0.001f;

                const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                pSpatialNode->setRelativeRotation(glm::vec3(5.0F, 0.0F, 0.0F));

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeRotation(glm::vec3(5.0F, 0.0F, 0.0F));

                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(
                    pSpatialNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::RESET_RELATIVE);

                const auto relativeRotation = pSpatialNode->getRelativeRotation();
                const auto worldRotation = pSpatialNode->getWorldRotation();
                REQUIRE(
                    glm::all(glm::epsilonEqual(relativeRotation, glm::vec3(0.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(worldRotation, glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("use `keepRelative` attachment rule for rotation") {
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

                float floatEpsilon = 0.001f;

                const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                pSpatialNode->setRelativeRotation(glm::vec3(5.0F, 0.0F, 0.0F));

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeRotation(glm::vec3(5.0F, 0.0F, 0.0F));

                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(
                    pSpatialNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

                const auto relativeRotation = pSpatialNode->getRelativeRotation();
                const auto worldRotation = pSpatialNode->getWorldRotation();
                REQUIRE(
                    glm::all(glm::epsilonEqual(relativeRotation, glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(worldRotation, glm::vec3(10.0f, 0.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("use `keepWorld` attachment rule for rotation") {
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

                float floatEpsilon = 0.001f;

                const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                pSpatialNode->setRelativeRotation(glm::vec3(5.0F, 0.0F, 0.0F));

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeRotation(glm::vec3(5.0F, 0.0F, 0.0F));

                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(
                    pSpatialNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_WORLD);

                const auto relativeRotation = pSpatialNode->getRelativeRotation();
                const auto worldRotation = pSpatialNode->getWorldRotation();
                REQUIRE(
                    glm::all(glm::epsilonEqual(relativeRotation, glm::vec3(0.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(
                    glm::all(glm::epsilonEqual(worldRotation, glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("use `reset` attachment rule for scale") {
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

                float floatEpsilon = 0.001f;

                const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                pSpatialNode->setRelativeScale(glm::vec3(5.0F, 0.0F, 0.0F));

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeScale(glm::vec3(5.0F, 0.0F, 0.0F));

                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(
                    pSpatialNode,
                    Node::AttachmentRule::RESET_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::RESET_RELATIVE);

                const auto relativeScale = pSpatialNode->getRelativeScale();
                const auto worldScale = pSpatialNode->getWorldScale();
                REQUIRE(
                    glm::all(glm::epsilonEqual(relativeScale, glm::vec3(1.0f, 1.0f, 1.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(worldScale, glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("use `keepRelative` attachment rule for scale") {
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

                float floatEpsilon = 0.001f;

                const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                pSpatialNode->setRelativeScale(glm::vec3(5.0F, 0.0F, 0.0F));

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeScale(glm::vec3(5.0F, 0.0F, 0.0F));

                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(
                    pSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE);

                const auto relativeScale = pSpatialNode->getRelativeScale();
                const auto worldScale = pSpatialNode->getWorldScale();
                REQUIRE(
                    glm::all(glm::epsilonEqual(relativeScale, glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(worldScale, glm::vec3(25.0f, 0.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("use `keepWorld` attachment rule for scale") {
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

                float floatEpsilon = 0.001f;

                const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                pSpatialNode->setRelativeScale(glm::vec3(5.0F, 0.0F, 0.0F));

                const auto pParentSpatialNode = sgc::makeGc<SpatialNode>();
                pParentSpatialNode->setRelativeScale(glm::vec3(5.0F, 0.0F, 0.0F));

                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(
                    pSpatialNode,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_RELATIVE,
                    Node::AttachmentRule::KEEP_WORLD);

                const auto relativeScale = pSpatialNode->getRelativeScale();
                const auto worldScale = pSpatialNode->getWorldScale();
                REQUIRE(
                    glm::all(glm::epsilonEqual(relativeScale, glm::vec3(1.0f, 0.0f, 0.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(worldScale, glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("serialize and deserialize spatial node tree") {
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

                float floatEpsilon = 0.001f;

                // Prepare paths.
                const std::filesystem::path pathToFile =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                    "TESTING_SpatialNodeTree_TESTING"; // not specifying ".toml" on purpose
                const std::filesystem::path fullPathToFile =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                    "TESTING_SpatialNodeTree_TESTING.toml";

                {
                    // Create nodes.
                    const auto pSpatialNode = sgc::makeGc<SpatialNode>();
                    const auto pChildSpatialNode = sgc::makeGc<SpatialNode>();

                    // Build hierarchy.
                    getWorldRootNode()->addChildNode(pSpatialNode);
                    pSpatialNode->addChildNode(pChildSpatialNode);

                    // Set locations.
                    pSpatialNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));
                    pChildSpatialNode->setRelativeLocation(glm::vec3(5.0F, 0.0F, 0.0F));

                    // Make sure locations are correct.
                    REQUIRE(glm::all(glm::epsilonEqual(
                        pSpatialNode->getRelativeLocation(), glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(
                        pChildSpatialNode->getRelativeLocation(),
                        glm::vec3(5.0f, 0.0f, 0.0f),
                        floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(
                        pSpatialNode->getWorldLocation(), glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(
                        pChildSpatialNode->getWorldLocation(), glm::vec3(10.0f, 0.0f, 0.0f), floatEpsilon)));

                    // Serialize.
                    const auto optionalError = getWorldRootNode()->serializeNodeTree(pathToFile, false);
                    if (optionalError.has_value()) {
                        auto err = optionalError.value();
                        err.addCurrentLocationToErrorStack();
                        INFO(err.getFullErrorMessage());
                        REQUIRE(false);
                    }

                    REQUIRE(std::filesystem::exists(fullPathToFile));
                }

                {
                    // Deserialize.
                    const auto deserializeResult = Node::deserializeNodeTree(pathToFile);
                    if (std::holds_alternative<Error>(deserializeResult)) {
                        auto err = std::get<Error>(deserializeResult);
                        err.addCurrentLocationToErrorStack();
                        INFO(err.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    const auto pRootNode = std::get<sgc::GcPtr<Node>>(deserializeResult);

                    // Check results.
                    auto pMtxChildNodes = pRootNode->getChildNodes();
                    {
                        std::scoped_lock childNodesGuard(pMtxChildNodes->first);
                        REQUIRE(pMtxChildNodes->second.size() == 1);
                    }

                    // Check child nodes.
                    const sgc::GcPtr<SpatialNode> pSpatialNode =
                        dynamic_cast<SpatialNode*>(pMtxChildNodes->second[0].get());
                    pMtxChildNodes = pSpatialNode->getChildNodes();
                    {
                        std::scoped_lock childNodesGuard(pMtxChildNodes->first);
                        REQUIRE(pMtxChildNodes->second.size() == 1);
                    }
                    const auto pChildSpatialNode =
                        dynamic_cast<SpatialNode*>(pMtxChildNodes->second[0].get());
                    pMtxChildNodes = pChildSpatialNode->getChildNodes();
                    {
                        std::scoped_lock childNodesGuard(pMtxChildNodes->first);
                        REQUIRE(pMtxChildNodes->second.size() == 0);
                    }

                    // Make sure locations are correct.
                    REQUIRE(glm::all(glm::epsilonEqual(
                        pSpatialNode->getRelativeLocation(), glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(
                        pChildSpatialNode->getRelativeLocation(),
                        glm::vec3(5.0f, 0.0f, 0.0f),
                        floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(
                        pSpatialNode->getWorldLocation(), glm::vec3(5.0f, 0.0f, 0.0f), floatEpsilon)));
                    REQUIRE(glm::all(glm::epsilonEqual(
                        pChildSpatialNode->getWorldLocation(), glm::vec3(10.0f, 0.0f, 0.0f), floatEpsilon)));
                }

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}

TEST_CASE("using `reset`/`keepRelative`/`keepWorld` attachment rule for location triggers "
          "onWorldLocationRotationScaleChanged on attach") {
    using namespace ne;

    class MyNode : public SpatialNode {
    public:
        virtual ~MyNode() override = default;

        bool bOnWorldLocationRotationScaleChanged = false;

    protected:
        /**
         * Called after node's world location/rotation/scale was changed.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         *
         * @remark If you change location/rotation/scale inside of this function,
         * this function will not be called again (no recursion will occur).
         */
        virtual void onWorldLocationRotationScaleChanged() override {
            SpatialNode::onWorldLocationRotationScaleChanged();

            bOnWorldLocationRotationScaleChanged = true;
        };
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

                const auto pSpatialNode1 = sgc::makeGc<MyNode>();
                const auto pSpatialNode2 = sgc::makeGc<MyNode>();
                const auto pSpatialNode3 = sgc::makeGc<MyNode>();

                REQUIRE(!pSpatialNode1->bOnWorldLocationRotationScaleChanged);
                REQUIRE(!pSpatialNode2->bOnWorldLocationRotationScaleChanged);
                REQUIRE(!pSpatialNode3->bOnWorldLocationRotationScaleChanged);

                getWorldRootNode()->addChildNode(pSpatialNode1, Node::AttachmentRule::RESET_RELATIVE);
                getWorldRootNode()->addChildNode(pSpatialNode2, Node::AttachmentRule::KEEP_RELATIVE);
                getWorldRootNode()->addChildNode(pSpatialNode3, Node::AttachmentRule::KEEP_WORLD);

                REQUIRE(pSpatialNode1->bOnWorldLocationRotationScaleChanged);
                REQUIRE(pSpatialNode2->bOnWorldLocationRotationScaleChanged);
                REQUIRE(pSpatialNode3->bOnWorldLocationRotationScaleChanged);

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

    // Make sure everything is collected correctly.
    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
