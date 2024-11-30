// Custom.
#include "material/Material.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/node/MeshNode.h"
#include "render/directx/pipeline/DirectXPso.h"
#include "render/directx/DirectXRenderer.h"
#include "shader/general/EngineShaderNames.hpp"
#include "shader/hlsl/RootSignatureGenerator.h"
#include "shader/ShaderManager.h"
#include "shader/hlsl/HlslShader.h"

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
                const auto pMeshNode = sgc::makeGc<MeshNode>();
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

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
}

TEST_CASE("root signature merge fails if vertex/pixel shaders have conflicting root constants") {
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

                const auto vertexShader = ShaderDescription(
                    "test.meshnode.vs",
                    "res/test/shaders/hlsl/conflicting_root_constants/vert.hlsl",
                    ShaderType::VERTEX_SHADER,
                    VertexFormat::MESH_NODE,
                    "main",
                    {});
                const auto correctFragmentShader = ShaderDescription(
                    "test.meshnode.correct.fs",
                    "res/test/shaders/hlsl/conflicting_root_constants/correct.frag.hlsl",
                    ShaderType::FRAGMENT_SHADER,
                    VertexFormat::MESH_NODE,
                    "main",
                    {});
                const auto conflictingFragmentShader = ShaderDescription(
                    "test.meshnode.conflict.fs",
                    "res/test/shaders/hlsl/conflicting_root_constants/conflict.frag.hlsl",
                    ShaderType::FRAGMENT_SHADER,
                    VertexFormat::MESH_NODE,
                    "main",
                    {});

                // Cast renderer.
                const auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer());
                REQUIRE(pDirectXRenderer != nullptr);

                // Compile vertex shader.
                auto result = ShaderPack::compileShaderPack(pDirectXRenderer, vertexShader);
                REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(result));
                const auto pVertexShaderPack = std::get<std::shared_ptr<ShaderPack>>(std::move(result));

                // Compile correct fragment shader.
                result = ShaderPack::compileShaderPack(pDirectXRenderer, correctFragmentShader);
                REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(result));
                const auto pCorrectFragmentShaderPack =
                    std::get<std::shared_ptr<ShaderPack>>(std::move(result));

                // Compile conflicting fragment shader.
                result = ShaderPack::compileShaderPack(pDirectXRenderer, conflictingFragmentShader);
                REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(result));
                auto pConflictingFragmentShaderPack =
                    std::get<std::shared_ptr<ShaderPack>>(std::move(result));

                // Get shaders.
                std::set<ShaderMacro> fullShaderConfiguration;
                const auto pVertexShader = pVertexShaderPack->getShader({});
                const auto pCorrectFragmentShader = pCorrectFragmentShaderPack->getShader({});
                const auto pConflictingFragmentShader = pConflictingFragmentShaderPack->getShader({});

                // Load reflection.
                dynamic_cast<HlslShader*>(pVertexShader.get())->getCompiledBlob();
                dynamic_cast<HlslShader*>(pCorrectFragmentShader.get())->getCompiledBlob();
                dynamic_cast<HlslShader*>(pConflictingFragmentShader.get())->getCompiledBlob();

                // Successfully generate a root signature with the same root parameters.
                auto rootSignatureResult = RootSignatureGenerator::generateGraphics(
                    pDirectXRenderer,
                    dynamic_cast<HlslShader*>(pVertexShader.get()),
                    dynamic_cast<HlslShader*>(pCorrectFragmentShader.get()));
                REQUIRE(!std::holds_alternative<Error>(rootSignatureResult));

                // Fail to generate a root signature with conflicting root parameters.
                rootSignatureResult = RootSignatureGenerator::generateGraphics(
                    pDirectXRenderer,
                    dynamic_cast<HlslShader*>(pVertexShader.get()),
                    dynamic_cast<HlslShader*>(pConflictingFragmentShader.get()));
                REQUIRE(std::holds_alternative<Error>(rootSignatureResult));

                // Release shader data.
                pVertexShader->releaseShaderDataFromMemoryIfLoaded();
                pCorrectFragmentShader->releaseShaderDataFromMemoryIfLoaded();
                pConflictingFragmentShader->releaseShaderDataFromMemoryIfLoaded();

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
