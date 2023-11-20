// Custom.
#include "material/Material.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/nodes/MeshNode.h"
#include "render/general/pipeline/PipelineManager.h"
#include "shader/ComputeShaderInterface.h"
#include "render/vulkan/VulkanRenderer.h"
#include "game/camera/TransientCamera.h"
#include "misc/PrimitiveMeshGenerator.h"
#include "game/nodes/light/PointLightNode.h"
#include "game/camera/CameraManager.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif

// External.
#include "catch2/catch_test_macros.hpp"

constexpr std::string_view sSampleHlslComputeShader = "[numthreads(1, 1, 1)]\n"
                                                      "void cs(){}\n";

constexpr std::string_view sSampleGlslComputeShader = "#version 450\n"
                                                      "layout (local_size_x = 128) in;\n"
                                                      "shared float foobar [128];\n"
                                                      "void main(){\n"
                                                      "foobar [gl_LocalInvocationIndex] = 0.0;\n"
                                                      "}\n";

TEST_CASE("manager correctly manages compute pipelines") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            // Prepare shader path.
            const std::filesystem::path shaderPathNoExtension =
                ne::ProjectPaths::getPathToResDirectory(ne::ResourceDirectory::ROOT) / "test" / "temp" /
                "test_compute_shader";

            // Prepare a compute shader.
            ShaderDescription computeShader;
#if defined(WIN32)
            if (dynamic_cast<DirectXRenderer*>(getWindow()->getRenderer()) != nullptr) {
                std::ofstream shaderFile(shaderPathNoExtension.string() + ".hlsl");
                REQUIRE(shaderFile.is_open());
                shaderFile << sSampleHlslComputeShader;
                shaderFile.close();

                computeShader = ShaderDescription(
                    "test.compute",
                    shaderPathNoExtension.string() + ".hlsl",
                    ShaderType::COMPUTE_SHADER,
                    "cs",
                    {});
            }
#endif
            if (dynamic_cast<VulkanRenderer*>(getWindow()->getRenderer()) != nullptr) {
                std::ofstream shaderFile(shaderPathNoExtension.string() + ".comp");
                REQUIRE(shaderFile.is_open());
                shaderFile << sSampleGlslComputeShader;
                shaderFile.close();

                computeShader = ShaderDescription(
                    "test.compute",
                    shaderPathNoExtension.string() + ".comp",
                    ShaderType::COMPUTE_SHADER,
                    "main",
                    {});
            }

            // Compile shader.
            auto optionalError = getWindow()->getRenderer()->getShaderManager()->compileShaders(
                {computeShader},
                [](size_t, size_t) {},
                [](ShaderDescription desc, std::variant<std::string, Error>) { REQUIRE(false); },
                [this]() {
                    // Create world.
                    createWorld([this](const std::optional<Error>& optionalWorldError) {
                        if (optionalWorldError.has_value()) [[unlikely]] {
                            auto error = optionalWorldError.value();
                            error.addCurrentLocationToErrorStack();
                            error.showError();
                            throw std::runtime_error(error.getFullErrorMessage());
                        }

                        // Create and setup camera.
                        auto pCamera = std::make_shared<TransientCamera>();
                        pCamera->setLocation(glm::vec3(-2.0F, 0.0F, 0.0F));

                        // Make it active.
                        getCameraManager()->setActiveCamera(pCamera);

                        // Spawn point light.
                        const auto pPointLightNode = gc_new<PointLightNode>();
                        getWorldRootNode()->addChildNode(pPointLightNode);
                        pPointLightNode->setWorldLocation(
                            glm::vec3(-1.0F, 5.0F, 5.0F)); // NOLINT: magic numbers

                        // Spawn sample mesh.
                        const auto pMeshNode = gc_new<MeshNode>();
                        pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                        getWorldRootNode()->addChildNode(pMeshNode);
                        pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

                        // Get pipeline manager.
                        const auto pPipelineManager = getWindow()->getRenderer()->getPipelineManager();

                        // Create compute interface.
                        auto result = ComputeShaderInterface::createUsingGraphicsQueue(
                            getWindow()->getRenderer(),
                            "test.compute",
                            ComputeExecutionStage::AFTER_DEPTH_PREPASS,
                            ComputeExecutionGroup::SECOND);
                        if (std::holds_alternative<Error>(result)) {
                            auto error = std::get<Error>(std::move(result));
                            error.addCurrentLocationToErrorStack();
                            error.showError();
                            throw std::runtime_error(error.getFullErrorMessage());
                        }
                        pComputeInterface =
                            std::get<std::unique_ptr<ComputeShaderInterface>>(std::move(result));

                        // Make sure a new compute pipeline was created.
                        REQUIRE(pPipelineManager->getCurrentComputePipelineCount() == 1);

                        // Create a new compute interface using the same shader.
                        result = ComputeShaderInterface::createUsingGraphicsQueue(
                            getWindow()->getRenderer(),
                            "test.compute",
                            ComputeExecutionStage::AFTER_DEPTH_PREPASS,
                            ComputeExecutionGroup::FIRST);
                        if (std::holds_alternative<Error>(result)) {
                            auto error = std::get<Error>(std::move(result));
                            error.addCurrentLocationToErrorStack();
                            error.showError();
                            throw std::runtime_error(error.getFullErrorMessage());
                        }
                        pTestInterface = std::get<std::unique_ptr<ComputeShaderInterface>>(std::move(result));

                        // Same pipeline.
                        REQUIRE(pPipelineManager->getCurrentComputePipelineCount() == 1);

                        // Make sure no compute shaders are queued yet.
                        auto mtxQueued = pPipelineManager->getComputeShadersForGraphicsQueueExecution();
                        for (const auto& stage : mtxQueued.second->vGraphicsQueueStagesGroups) {
                            for (const auto& group : stage) {
                                REQUIRE(group.empty());
                            }
                        }

                        // Submit our shader.
                        pTestInterface->submitForExecution(1, 1, 1);

                        // Prepare some variables.
                        auto& afterDepthPrepassGroups =
                            mtxQueued.second->vGraphicsQueueStagesGroups[static_cast<size_t>(
                                ComputeExecutionStage::AFTER_DEPTH_PREPASS)];
                        const auto iFirstGroupIndex = static_cast<size_t>(ComputeExecutionGroup::FIRST);
                        const auto iSecondGroupIndex = static_cast<size_t>(ComputeExecutionGroup::SECOND);

                        // Check queue.
                        REQUIRE(afterDepthPrepassGroups[iFirstGroupIndex].size() == 1);
                        REQUIRE(afterDepthPrepassGroups[iSecondGroupIndex].empty());

                        // Submit other shader.
                        pComputeInterface->submitForExecution(1, 1, 1);

                        // Check queue.
                        REQUIRE(afterDepthPrepassGroups[iFirstGroupIndex].size() == 1);
                        REQUIRE(afterDepthPrepassGroups[iSecondGroupIndex].size() == 1);

                        // Destroy all compute interfaces.
                        pComputeInterface = nullptr;
                        pTestInterface = nullptr;

                        // All new pipelines should be destroyed.
                        REQUIRE(pPipelineManager->getCurrentComputePipelineCount() == 0);

                        // Check queue.
                        for (const auto& stage : mtxQueued.second->vGraphicsQueueStagesGroups) {
                            for (const auto& group : stage) {
                                REQUIRE(group.empty());
                            }
                        }

                        getWindow()->close();
                    });
                });
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                INFO(optionalError->getFullErrorMessage());
                REQUIRE(false);
            }
        }
        virtual ~TestGameInstance() override {}

    private:
        std::unique_ptr<ComputeShaderInterface> pComputeInterface;
        std::unique_ptr<ComputeShaderInterface> pTestInterface;
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
