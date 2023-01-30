// Custom.
#include "game/nodes/SpatialNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("world location, rotation and scale are calculated correctly (no parent)") {

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

                constexpr float floatEpsilon = 0.00001f;

                const auto targetWorldLocation = glm::vec3(1.0f, 2.0f, 3.0f);
                const auto targetWorldRotation = glm::vec3(10.0f, 20.0f, 30.0f);
                const auto targetWorldScale = glm::vec3(5.0f, 6.0f, 7.0f);

                const auto pSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");
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
        error.addEntry();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("world location, rotation and scale are calculated correctly (with parent)") {
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

                constexpr float floatEpsilon = 0.00001f;

                const auto pParentSpatialNode = gc_new<SpatialNode>("My Cool Parent Spatial Node");
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pParentSpatialNode->setRelativeScale(glm::vec3(5.0f, 1.0f, 1.0f));

                const auto pChildSpatialNode = gc_new<SpatialNode>("My Cool Child Spatial Node");
                pParentSpatialNode->addChildNode(pChildSpatialNode);
                pChildSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pChildSpatialNode->setRelativeScale(glm::vec3(1.0f, 1.0f, 5.0f));

                const auto worldLocation = pChildSpatialNode->getWorldLocation();
                const auto worldRotation = pChildSpatialNode->getWorldRotation();
                const auto worldScale = pChildSpatialNode->getWorldScale();

                REQUIRE(
                    glm::all(glm::epsilonEqual(worldLocation, glm::vec3(10.0f, 0.0f, 0.0f), floatEpsilon)));
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
        error.addEntry();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE(
    "world location, rotation and scale are calculated correctly (with non spatial nodes in the hierarchy)") {
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

                constexpr float floatEpsilon = 0.00001f;

                // Create nodes.
                const auto pParentSpatialNode = gc_new<SpatialNode>("My Cool Parent Spatial Node");
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pParentSpatialNode->setRelativeScale(glm::vec3(5.0f, 1.0f, 1.0f));

                const auto pUsualNode1 = gc_new<Node>("Usual Node 1");

                const auto pSpatialNode = gc_new<SpatialNode>("My Cool Child Spatial Node");

                const auto pUsualNode2 = gc_new<Node>("Usual Node 2");

                const auto pChildSpatialNode = gc_new<SpatialNode>("My Cool Child Spatial Node 1");
                pChildSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pChildSpatialNode->setRelativeScale(glm::vec3(1.0f, 1.0f, 5.0f));

                const auto pUsualNode3 = gc_new<Node>("Usual Node 3");

                const auto pChildChildSpatialNode = gc_new<SpatialNode>("My Cool Child Spatial Node 2");
                pChildChildSpatialNode->setRelativeLocation(glm::vec3(5.0f, 0.0f, 0.0f));
                pChildChildSpatialNode->setRelativeScale(glm::vec3(1.0f, 1.0f, 5.0f));

                // Build hierarchy.
                pParentSpatialNode->addChildNode(pUsualNode1);
                pUsualNode1->addChildNode(pSpatialNode);
                pSpatialNode->addChildNode(pUsualNode2);
                pUsualNode2->addChildNode(pChildSpatialNode);
                pChildSpatialNode->addChildNode(pUsualNode3);
                pUsualNode3->addChildNode(pChildChildSpatialNode);

                // Check locations.
                const auto worldLocation = pChildChildSpatialNode->getWorldLocation();
                const auto worldRotation = pChildChildSpatialNode->getWorldRotation();
                const auto worldScale = pChildChildSpatialNode->getWorldScale();

                REQUIRE(
                    glm::all(glm::epsilonEqual(worldLocation, glm::vec3(15.0f, 0.0f, 0.0f), floatEpsilon)));
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
        error.addEntry();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("world location with parent rotation is correct") {
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

                float floatEpsilon = 0.00001f;

                // Create nodes.
                const auto pParentSpatialNodeA = gc_new<SpatialNode>();
                pParentSpatialNodeA->setRelativeRotation(glm::vec3(0.0f, 0.0f, 45.0f));

                const auto pParentSpatialNodeB = gc_new<SpatialNode>();
                pParentSpatialNodeB->setRelativeRotation(glm::vec3(90.0f, 0.0f, 0.0f));

                const auto pSpatialNodeA = gc_new<SpatialNode>();
                pSpatialNodeA->setRelativeRotation(glm::vec3(0.0f, 0.0f, 45.0f));
                pSpatialNodeA->setRelativeLocation(glm::vec3(10.0f, 0.0f, 0.0f));

                const auto pSpatialNodeB = gc_new<SpatialNode>();
                pSpatialNodeB->setRelativeRotation(glm::vec3(0.0f, 0.0f, 45.0f));

                const auto pSpatialNodeC = gc_new<SpatialNode>();
                pSpatialNodeC->setRelativeRotation(glm::vec3(0.0f, 0.0f, 90.0f));

                const auto pChildSpatialNodeA = gc_new<SpatialNode>();
                const auto pChildSpatialNodeB = gc_new<SpatialNode>();
                pChildSpatialNodeB->setRelativeLocation(glm::vec3(10.0f, 0.0f, 0.0f));

                const auto pChildSpatialNodeC = gc_new<SpatialNode>();
                pChildSpatialNodeC->setRelativeLocation(glm::vec3(0.0f, 10.0f, 0.0f));

                // Build hierarchy.
                pParentSpatialNodeA->addChildNode(pSpatialNodeA);
                pParentSpatialNodeA->addChildNode(pSpatialNodeB);
                pParentSpatialNodeB->addChildNode(pSpatialNodeC);
                pSpatialNodeA->addChildNode(pChildSpatialNodeA);
                pSpatialNodeB->addChildNode(pChildSpatialNodeB);
                pSpatialNodeC->addChildNode(pChildSpatialNodeC);
                getWorldRootNode()->addChildNode(pParentSpatialNodeA);
                getWorldRootNode()->addChildNode(pParentSpatialNodeB);

                // Check location.
                const auto middleANodeWorldLocation = pSpatialNodeA->getWorldLocation();
                const auto childANodeWorldLocation = pChildSpatialNodeA->getWorldLocation();
                const auto childBNodeWorldLocation = pChildSpatialNodeB->getWorldLocation();
                const auto childCNodeWorldLocation = pChildSpatialNodeC->getWorldLocation();
                REQUIRE(glm::all(glm::epsilonEqual(
                    middleANodeWorldLocation, glm::vec3(7.07106f, -7.07106f, 0.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    childANodeWorldLocation, glm::vec3(7.07106f, -7.07106f, 0.0f), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childBNodeWorldLocation, glm::vec3(0.0f, -10.0f, 0.0f), floatEpsilon)));
                REQUIRE(glm::all(
                    glm::epsilonEqual(childCNodeWorldLocation, glm::vec3(10.0f, 0.0f, 0.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("set world location with parent is correct") {
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

                constexpr float floatEpsilon = 0.00001f;

                // Create nodes.
                const auto pParentSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");
                pParentSpatialNode->setRelativeLocation(glm::vec3(5.0f, 5.0f, 5.0f));

                const auto pUsualNode = gc_new<Node>("Usual Node");

                const auto pChildSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");

                // Build hierarchy.
                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(pUsualNode);
                pUsualNode->addChildNode(pChildSpatialNode);

                // Set world location.
                pChildSpatialNode->setWorldLocation(glm::vec3(-5.0f, -5.0f, -5.0f));

                // Check locations.
                REQUIRE(glm::all(glm::epsilonEqual(
                    pParentSpatialNode->getWorldLocation(), glm::vec3(5.0f, 5.0f, 5.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildSpatialNode->getRelativeLocation(),
                    glm::vec3(-10.0f, -10.0f, -10.0f),
                    floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildSpatialNode->getWorldLocation(), glm::vec3(-5.0f, -5.0f, -5.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("set world rotation with parent is correct") {
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

                float floatEpsilon = 0.00001f;

                // Create nodes.
                const auto pParentSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");
                pParentSpatialNode->setRelativeRotation(glm::vec3(1.0f, 5.0f, 10.0f));

                const auto pUsualNode = gc_new<Node>("Usual Node");

                const auto pChildSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");

                // Build hierarchy.
                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(pUsualNode);
                pUsualNode->addChildNode(pChildSpatialNode);

                // Set world rotation.
                pChildSpatialNode->setWorldRotation(glm::vec3(-1.0f, -5.0f, -10.0f));

                // Check rotations.
                REQUIRE(glm::all(glm::epsilonEqual(
                    pParentSpatialNode->getWorldRotation(), glm::vec3(1.0f, 5.0f, 10.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildSpatialNode->getRelativeRotation(),
                    glm::vec3(-2.0f, -10.0f, -20.0f),
                    floatEpsilon)));

                floatEpsilon = 3.0f; // degrees
                const auto worldRotation = pChildSpatialNode->getWorldRotation();
                REQUIRE(glm::all(
                    glm::epsilonEqual(worldRotation, glm::vec3(-1.0f, -5.0f, -10.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("set world scale with parent is correct") {
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

                constexpr float floatEpsilon = 0.00001f;

                // Create nodes.
                const auto pParentSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");
                pParentSpatialNode->setRelativeScale(glm::vec3(5.0f, 5.0f, 5.0f));

                const auto pUsualNode = gc_new<Node>("Usual Node");

                const auto pChildSpatialNode = gc_new<SpatialNode>("My Cool Spatial Node");

                // Build hierarchy.
                getWorldRootNode()->addChildNode(pParentSpatialNode);
                pParentSpatialNode->addChildNode(pUsualNode);
                pUsualNode->addChildNode(pChildSpatialNode);

                // Set world scale.
                pChildSpatialNode->setWorldScale(glm::vec3(2.0f, 2.0f, 2.0f));

                // Check scale.
                REQUIRE(glm::all(glm::epsilonEqual(
                    pParentSpatialNode->getWorldScale(), glm::vec3(5.0f, 5.0f, 5.0f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildSpatialNode->getRelativeScale(), glm::vec3(0.4f, 0.4f, 0.4f), floatEpsilon)));
                REQUIRE(glm::all(glm::epsilonEqual(
                    pChildSpatialNode->getWorldScale(), glm::vec3(2.0f, 2.0f, 2.0f), floatEpsilon)));

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

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}

TEST_CASE("serialize and deserialize SpatialNode") {
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

                const std::filesystem::path pathToFileInTemp =
                    ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / "test" / "temp" /
                    "TESTING_SpatialNodeSerialization_TESTING.toml";

                const auto location = glm::vec3(1.0f, -2.0f, 3.0f);
                const auto rotation = glm::vec3(-5.0f, 15.0f, -30.0f);
                const auto scale = glm::vec3(10.0f, 20.0f, 30.0f);

                {
                    // Setup.
                    const auto pSpatialNode = gc_new<SpatialNode>();
                    pSpatialNode->setRelativeLocation(location);
                    pSpatialNode->setRelativeRotation(rotation);
                    pSpatialNode->setRelativeScale(scale);

                    // Serialize.
                    const auto optionalError = pSpatialNode->serialize(pathToFileInTemp, false);
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                {
                    // Deserialize.
                    auto result = Serializable::deserialize<gc, SpatialNode>(pathToFileInTemp);
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    const auto pSpatialNode = std::get<gc<SpatialNode>>(std::move(result));

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
        error.addEntry();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    // Make sure everything is collected correctly.
    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
}
