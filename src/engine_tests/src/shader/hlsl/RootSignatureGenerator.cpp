// Custom.
#include "material/Material.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/nodes/MeshNode.h"
#include "render/directx/pipeline/DirectXPso.h"
#include "render/directx/DirectXRenderer.h"
#include "shader/general/EngineShaderNames.hpp"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("root signature merge is correct") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

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
                const auto pPso = dynamic_cast<DirectXPso*>(pMeshNode->getMaterial()->getColorPipeline());
                if (pPso == nullptr) {
                    INFO("expected a DirectX renderer");
                    REQUIRE(false);
                }

                const auto pMtxPsoInternalResources = pPso->getInternalResources();
                {
                    std::scoped_lock guard(pMtxPsoInternalResources->first);
                    auto& params = pMtxPsoInternalResources->second.rootParameterIndices;

                    REQUIRE(params.size() >= 3);

                    auto it = params.find("frameData");
                    REQUIRE(it != params.end());

                    it = params.find("meshData");
                    REQUIRE(it != params.end());

                    it = params.find("materialData");
                    REQUIRE(it != params.end());
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
}
