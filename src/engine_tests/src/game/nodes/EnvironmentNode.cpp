// Custom.
#include "game/nodes/CameraNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "misc/Globals.h"
#include "game/nodes/EnvironmentNode.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("serialize and deserialize EnvironmentNode as part of a node tree") {
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
                    "TESTING_EnvironmentNodeTreeSerialization_TESTING.toml";
                const auto ambientLight = glm::vec3(0.5F, 0.1F, 0.0F);

                {
                    // Create node and initialize.
                    const auto pEnvironmentNode = sgc::makeGc<EnvironmentNode>();
                    pEnvironmentNode->setAmbientLight(ambientLight);
                    getWorldRootNode()->addChildNode(pEnvironmentNode);

                    // Serialize tree.
                    auto optionalError = getWorldRootNode()->serializeNodeTree(pathToFileInTemp, false);
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                sgc::GarbageCollector::get().collectGarbage();

                {
                    // Deserialize.
                    auto result = Node::deserializeNodeTree(pathToFileInTemp);
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    const auto pRootNode = std::get<sgc::GcPtr<Node>>(std::move(result));

                    REQUIRE(pRootNode->getChildNodes()->second.size() == 1);
                    const sgc::GcPtr<EnvironmentNode> pEnvironmentNode =
                        dynamic_cast<EnvironmentNode*>(pRootNode->getChildNodes()->second[0].get());

                    // Check.
                    REQUIRE(glm::all(
                        glm::epsilonEqual(pEnvironmentNode->getAmbientLight(), ambientLight, 0.01F)));
                }

                // Cleanup.
                if (std::filesystem::exists(pathToFileInTemp)) {
                    ConfigManager::removeFile(pathToFileInTemp);
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

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
}
