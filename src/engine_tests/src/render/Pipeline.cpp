// Custom.
#include "material/Material.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/nodes/MeshNode.h"
#include "render/general/pipeline/Pipeline.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("make sure used vertex/pixel shader configuration of MeshNode is correct") {
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

                // Create sample mesh data.
                MeshData meshData;
                meshData.getVertices()->push_back(MeshVertex());
                meshData.getIndices()->push_back({0});

                // Create node and initialize.
                const auto pMeshNode = gc_new<MeshNode>();
                pMeshNode->setMeshData(std::move(meshData));

                // Spawn mesh node.
                getWorldRootNode()->addChildNode(pMeshNode);

                // Get initialized PSO.
                const auto pPso = pMeshNode->getMaterial()->getUsedPipeline();

                // Check vertex shader configuration.
                const auto optionalFullVertexShaderConfiguration =
                    pPso->getCurrentShaderConfiguration(ShaderType::VERTEX_SHADER);
                REQUIRE(optionalFullVertexShaderConfiguration.has_value());

                const auto fullVertexShaderConfiguration = optionalFullVertexShaderConfiguration.value();
                REQUIRE(fullVertexShaderConfiguration.empty());

                // Check pixel shader configuration.
                const auto optionalFullPixelShaderConfiguration =
                    pPso->getCurrentShaderConfiguration(ShaderType::FRAGMENT_SHADER);
                REQUIRE(optionalFullPixelShaderConfiguration.has_value());

                const auto fullPixelShaderConfiguration = optionalFullPixelShaderConfiguration.value();
                REQUIRE(fullPixelShaderConfiguration.empty());

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
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
}
