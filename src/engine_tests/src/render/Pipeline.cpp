// Custom.
#include "material/Material.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/node/MeshNode.h"
#include "render/general/pipeline/Pipeline.h"
#include "render/general/pipeline/PipelineManager.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("there are only 2 shadow mapping pipelines per vertex shader") {
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

                // Create several nodes and spawn so that they would initialize (request) pipelines.
                getWorldRootNode()->addChildNode(sgc::makeGc<MeshNode>());
                getWorldRootNode()->addChildNode(sgc::makeGc<MeshNode>());
                getWorldRootNode()->addChildNode(sgc::makeGc<MeshNode>());

                // Get graphics pipelines.
                const auto pMtxPipelines =
                    getWindow()->getRenderer()->getPipelineManager()->getGraphicsPipelines();
                std::scoped_lock guard(pMtxPipelines->first);

                // Get shadow mapping pipelines.
                const auto& directionalSpotPipelines =
                    pMtxPipelines->second.vPipelineTypes[static_cast<size_t>(
                        GraphicsPipelineType::PT_SHADOW_MAPPING_DIRECTIONAL_SPOT)];
                const auto& pointPipelines =
                    pMtxPipelines->second
                        .vPipelineTypes[static_cast<size_t>(GraphicsPipelineType::PT_SHADOW_MAPPING_POINT)];

                // Check the number of pipelines.
                REQUIRE(directionalSpotPipelines.size() == 1);
                REQUIRE(pointPipelines.size() == 1);

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
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
}
