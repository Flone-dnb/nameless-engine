// Custom.
#include "material/Material.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/camera/TransientCamera.h"
#include "game/nodes/MeshNode.h"
#include "misc/ProjectPaths.h"
#include "shader/general/EngineShaderNames.hpp"
#include "misc/PrimitiveMeshGenerator.h"
#include "game/camera/CameraManager.h"
#include "shader/general/Shader.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif
#include "render/vulkan/VulkanRenderer.h"

// External.
#include "catch2/catch_test_macros.hpp"

#if defined(WIN32)
inline bool textureImportProcess(float percent, unsigned long long, unsigned long long) {
#else
inline bool textureImportProcess(float percent, int*, int*) {
#endif
    using namespace ne;

    Logger::get().info(std::format("importing texture, progress: {}", percent));

    return false;
}

static constexpr auto pImportedTexture1DirectoryName = "imported1";
static constexpr auto pImportedTexture2DirectoryName = "imported2";
static const std::string sImportedTexture1PathRelativeRes =
    std::string("test/temp/") + pImportedTexture1DirectoryName;
static const std::string sImportedTexture2PathRelativeRes =
    std::string("test/temp/") + pImportedTexture2DirectoryName;

/** Returns `true` if successful, `false` if failed. */
bool prepareDiffuseTextures() {
    using namespace ne;

    // Prepare some paths.

    const auto pathToImportexTexture1Dir = ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                                           "test" / "temp" / pImportedTexture1DirectoryName;
    const auto pathToImportexTexture2Dir = ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                                           "test" / "temp" / pImportedTexture2DirectoryName;

    // Delete previously imported texture (if exists).
    if (std::filesystem::exists(pathToImportexTexture1Dir)) {
        std::filesystem::remove_all(pathToImportexTexture1Dir);
    }
    if (std::filesystem::exists(pathToImportexTexture2Dir)) {
        std::filesystem::remove_all(pathToImportexTexture2Dir);
    }

    // Import sample texture.
    auto optionalError = TextureManager::importTexture(
        ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "texture.png",
        TextureType::DIFFUSE,
        "test/temp",
        pImportedTexture1DirectoryName,
        textureImportProcess);
    if (optionalError.has_value()) {
        optionalError->addCurrentLocationToErrorStack();
        INFO(optionalError->getFullErrorMessage());
        REQUIRE(false);
        return false;
    }

    optionalError = TextureManager::importTexture(
        ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "texture.png",
        TextureType::DIFFUSE,
        "test/temp",
        pImportedTexture2DirectoryName,
        textureImportProcess);
    if (optionalError.has_value()) {
        optionalError->addCurrentLocationToErrorStack();
        INFO(optionalError->getFullErrorMessage());
        REQUIRE(false);
        return false;
    }

    return true;
}

TEST_CASE("create engine default materials") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create material.
                auto resultOpaque = Material::create(
                    EngineShaderNames::MeshNode::sVertexShaderName,
                    EngineShaderNames::MeshNode::sPixelShaderName,
                    false);
                if (std::holds_alternative<Error>(resultOpaque)) {
                    Error error = std::get<Error>(std::move(resultOpaque));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                auto resultTransparent = Material::create(
                    EngineShaderNames::MeshNode::sVertexShaderName,
                    EngineShaderNames::MeshNode::sPixelShaderName,
                    true);
                if (std::holds_alternative<Error>(resultTransparent)) {
                    Error error = std::get<Error>(std::move(resultTransparent));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Prepare dummy mesh.
                MeshVertex vertex;
                MeshData meshData;
                meshData.getVertices()->push_back(vertex);
                meshData.getIndices()->push_back({0});

                // Create nodes.
                auto pMeshNodeTransparent = gc_new<MeshNode>("Transparent material node");
                pMeshNodeTransparent->setMaterial(
                    std::get<std::shared_ptr<Material>>(std::move(resultTransparent)));
                pMeshNodeTransparent->setMeshData(meshData);

                auto pMeshNodeOpaque = gc_new<MeshNode>("Opaque material node");
                pMeshNodeOpaque->setMaterial(std::get<std::shared_ptr<Material>>(std::move(resultOpaque)));
                pMeshNodeOpaque->setMeshData(meshData);

                // There should be no PSOs created since no material is spawned.
                REQUIRE(
                    getWindow()->getRenderer()->getPipelineManager()->getCurrentGraphicsPipelineCount() == 0);

                // Spawn.
                getWorldRootNode()->addChildNode(pMeshNodeOpaque);
                getWorldRootNode()->addChildNode(pMeshNodeTransparent);

                // Check everything.
                REQUIRE(Material::getCurrentAliveMaterialCount() == 2);
                REQUIRE(
                    getWindow()->getRenderer()->getPipelineManager()->getCurrentGraphicsPipelineCount() ==
                    3); // 1 opaque PSO + 1 depth only PSO + 1 transparent PSO

                // Despawn one node.
                pMeshNodeOpaque->detachFromParentAndDespawn();
                REQUIRE(Material::getCurrentAliveMaterialCount() == 2);
                REQUIRE(
                    getWindow()->getRenderer()->getPipelineManager()->getCurrentGraphicsPipelineCount() ==
                    1); // only transparent PSO

                // Despawn all nodes.
                createWorld([&](const std::optional<Error>& optionalError) {
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    // Check that everything is cleaned up.
                    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
                    REQUIRE(
                        getWindow()->getRenderer()->getPipelineManager()->getCurrentGraphicsPipelineCount() ==
                        0);

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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
}

TEST_CASE("serialize and deserialize Material") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                const std::filesystem::path pathToFileInTemp =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                    "TESTING_MaterialSerialization_TESTING.toml";

                const auto diffuseColor = glm::vec3(0.0F, 1.0F, 0.0F);
                const auto specularColor = glm::vec3(0.0F, 0.0F, 1.0F);
                const auto roughness = 0.9F;
                const auto opacity = 0.6F;

                {
                    // Create material.
                    auto result = Material::create(
                        EngineShaderNames::MeshNode::sVertexShaderName,
                        EngineShaderNames::MeshNode::sPixelShaderName,
                        true,
                        "My Material");
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    const auto pMaterial = std::get<std::shared_ptr<Material>>(std::move(result));

                    // Customize.
                    pMaterial->setDiffuseColor(diffuseColor);
                    pMaterial->setSpecularColor(specularColor);
                    pMaterial->setRoughness(roughness);
                    pMaterial->setOpacity(opacity);

                    // Serialize.
                    auto optionalError = pMaterial->serialize(pathToFileInTemp, false);
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                REQUIRE(Material::getCurrentAliveMaterialCount() == 0);

                {
                    // Deserialize.
                    auto result = Serializable::deserialize<std::shared_ptr, Material>(pathToFileInTemp);
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }

                    const auto pMaterial = std::get<std::shared_ptr<Material>>(std::move(result));

                    // Check.
                    REQUIRE(pMaterial->getMaterialName() == "My Material");
                    REQUIRE(pMaterial->isUsingTransparency());
                    REQUIRE(glm::all(glm::epsilonEqual(pMaterial->getDiffuseColor(), diffuseColor, 0.001F)));
                    REQUIRE(
                        glm::all(glm::epsilonEqual(pMaterial->getSpecularColor(), specularColor, 0.001F)));
                    REQUIRE(std::abs(pMaterial->getOpacity() - opacity) < 0.001F);
                    REQUIRE(std::abs(pMaterial->getRoughness() - roughness) < 0.001F);
                }

                REQUIRE(Material::getCurrentAliveMaterialCount() == 0);

                // Cleanup.
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
}

TEST_CASE("unused materials unload shaders from memory") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            // Make sure we are using DirectX renderer.
            if (dynamic_cast<VulkanRenderer*>(getWindow()->getRenderer()) != nullptr) {
                // Don't run this test on non-DirectX renderer.
                getWindow()->close();
                SKIP();
            }

            std::vector<ShaderDescription> vShadersToCompile = {
                ShaderDescription(
                    "test.custom_mesh_node.vs",
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                        "test/shaders/hlsl/CustomMeshNode.vert.hlsl",
                    ShaderType::VERTEX_SHADER,
                    "vsCustomMeshNode",
                    {}),
                ShaderDescription(
                    "test.custom_mesh_node.ps",
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                        "test/shaders/hlsl/CustomMeshNode.frag.hlsl",
                    ShaderType::PIXEL_SHADER,
                    "psCustomMeshNode",
                    {})};

            const auto optionalError = getWindow()->getRenderer()->getShaderManager()->compileShaders(
                std::move(vShadersToCompile),
                [](size_t iCompiled, size_t iTotal) {

                },
                [](ShaderDescription description, std::variant<std::string, Error> error) {
                    if (std::holds_alternative<Error>(error)) {
                        INFO(std::get<Error>(std::move(error)).getFullErrorMessage());
                    } else {
                        INFO(std::get<std::string>(std::move(error)));
                    }
                    REQUIRE(false);
                },
                [this]() {
                    createWorld([this](const std::optional<Error>& optionalError) {
                        if (optionalError.has_value()) {
                            auto error = optionalError.value();
                            error.addCurrentLocationToErrorStack();
                            INFO(error.getFullErrorMessage());
                            REQUIRE(false);
                        }

                        // Create custom material.
                        auto result =
                            Material::create("test.custom_mesh_node.vs", "test.custom_mesh_node.ps", false);
                        if (std::holds_alternative<Error>(result)) {
                            Error error = std::get<Error>(std::move(result));
                            error.addCurrentLocationToErrorStack();
                            INFO(error.getFullErrorMessage());
                            REQUIRE(false);
                        }

                        // Prepare dummy mesh.
                        MeshVertex vertex;
                        MeshData meshData;
                        meshData.getVertices()->push_back(vertex);
                        meshData.getIndices()->push_back({0});

                        // Create nodes with custom material.
                        auto pCustomMeshNode1 = gc_new<MeshNode>();
                        pCustomMeshNode1->setMaterial(std::get<std::shared_ptr<Material>>(std::move(result)));
                        pCustomMeshNode1->setMeshData(meshData);

                        // Create nodes with custom material.
                        auto pCustomMeshNode2 = gc_new<MeshNode>();
                        pCustomMeshNode2->setMaterial(pCustomMeshNode1->getMaterial());
                        pCustomMeshNode2->setMeshData(meshData);

                        // Create node with default material.
                        auto pMeshNode = gc_new<MeshNode>();
                        pMeshNode->setMeshData(meshData);

                        // Make sure there are no shaders loaded in the memory.
                        REQUIRE(Shader::getCurrentAmountOfShadersInMemory() == 0);

                        // Spawn.
                        getWorldRootNode()->addChildNode(pMeshNode);
                        getWorldRootNode()->addChildNode(pCustomMeshNode1);
                        getWorldRootNode()->addChildNode(pCustomMeshNode2);
                        REQUIRE(
                            Shader::getCurrentAmountOfShadersInMemory() == 4); // 2 vertex + 2 pixel shaders

                        // Despawn the first custom mesh.
                        pCustomMeshNode1->detachFromParentAndDespawn();
                        REQUIRE(Shader::getCurrentAmountOfShadersInMemory() == 4);

                        // Despawn the second custom mesh.
                        pCustomMeshNode2->detachFromParentAndDespawn();
                        REQUIRE(
                            Shader::getCurrentAmountOfShadersInMemory() == 2); // 1 vertex + 1 pixel shader

                        // Despawn default mesh.
                        pMeshNode->detachFromParentAndDespawn();
                        REQUIRE(Shader::getCurrentAmountOfShadersInMemory() == 0);

                        getWindow()->close();
                    });
                });
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addCurrentLocationToErrorStack();
                INFO(error.getFullErrorMessage());
                REQUIRE(false);
            }
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

TEST_CASE("2 meshes with 2 materials (different diffuse textures no transparency)") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Prepare textures.
                if (!prepareDiffuseTextures()) {
                    REQUIRE(false);
                }

                // Prepare pipeline manager.
                const auto pPipelineManager = getWindow()->getRenderer()->getPipelineManager();

                // No pipelines should exist.
                REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 0);

                // Spawn sample mesh 1.
                const auto pMeshNode1 = gc_new<MeshNode>();
                pMeshNode1->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));

                getWorldRootNode()->addChildNode(
                    pMeshNode1, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
                pMeshNode1->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

                // Set texture after spawning.
                pMeshNode1->getMaterial()->setDiffuseTexture(sImportedTexture1PathRelativeRes);

                // Spawn sample mesh 2.
                const auto pMeshNode2 = gc_new<MeshNode>();
                pMeshNode2->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));

                // Set texture before spawning.
                pMeshNode2->getMaterial()->setDiffuseTexture(sImportedTexture2PathRelativeRes);

                getWorldRootNode()->addChildNode(
                    pMeshNode2, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);
                pMeshNode2->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

                // Make sure textures are set.
                REQUIRE(!pMeshNode1->getMaterial()->getPathToDiffuseTextureResource().empty());
                REQUIRE(!pMeshNode2->getMaterial()->getPathToDiffuseTextureResource().empty());

                // Make sure texture are different.
                REQUIRE(
                    pMeshNode1->getMaterial()->getPathToDiffuseTextureResource() !=
                    pMeshNode2->getMaterial()->getPathToDiffuseTextureResource());

                // Only 2 pipelines should exist (1 opaque + 1 depth only pipeline) (2 materials with
                // different textures).
                REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 2);

                // Remove diffuse texture from one mesh.
                pMeshNode1->getMaterial()->setDiffuseTexture("");

                // Make sure texture is removed.
                REQUIRE(pMeshNode1->getMaterial()->getPathToDiffuseTextureResource().empty());

                // There should now be 3 pipelines (1 opaque no diffuse + 1 opaque with diffuse + 1 depth only
                // pipeline) (2 materials one with diffuse texture and one without).
                REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 3);

                // Now despawn 1 mesh.
                pMeshNode1->detachFromParentAndDespawn();

                // There should now be just 2 pipelines (1 opaque + 1 depth only).
                REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 2);

                // Despawn second mesh.
                pMeshNode2->detachFromParentAndDespawn();

                // No pipeline should exist now.
                REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 0);

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

TEST_CASE("make sure there are no transparency macros in opaque pipelines") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Prepare pipeline manager.
                const auto pPipelineManager = getWindow()->getRenderer()->getPipelineManager();

                // No pipelines should exist.
                REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 0);

                // Spawn sample mesh 1.
                const auto pMeshNode1 = gc_new<MeshNode>();
                auto mtxMeshData = pMeshNode1->getMeshData();
                {
                    std::scoped_lock guard(*mtxMeshData.first);
                    (*mtxMeshData.second) = PrimitiveMeshGenerator::createCube(1.0F);
                }

                getWorldRootNode()->addChildNode(pMeshNode1);
                pMeshNode1->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

                // Spawn sample mesh 2.
                const auto pMeshNode2 = gc_new<MeshNode>();
                mtxMeshData = pMeshNode2->getMeshData();
                {
                    std::scoped_lock guard(*mtxMeshData.first);
                    (*mtxMeshData.second) = PrimitiveMeshGenerator::createCube(1.0F);
                }

                // Create transparent material.
                auto result = Material::create(
                    EngineShaderNames::MeshNode::sVertexShaderName,
                    EngineShaderNames::MeshNode::sPixelShaderName,
                    true);
                if (std::holds_alternative<Error>(result)) {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Use transparent material.
                pMeshNode2->setMaterial(std::get<std::shared_ptr<Material>>(std::move(result)));

                getWorldRootNode()->addChildNode(pMeshNode2);
                pMeshNode2->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

                // There should now be 3 pipelines (2 materials one with transparency and one without)
                // (1 opaque pipeline + 1 depth only pipeline + 1 transparent pipeline).
                REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 3);

                {
                    const auto pMtxGraphicsPipelines = pPipelineManager->getGraphicsPipelines();
                    std::scoped_lock pipelineGuard(pMtxGraphicsPipelines->first);

                    auto& mtxOpaquePipelines =
                        pMtxGraphicsPipelines->second
                            .vPipelineTypes[static_cast<size_t>(PipelineType::PT_OPAQUE)];

                    // Make sure opaque pipelines don't have transparency macro defined.
                    for (const auto& [sShaderNames, pipelines] : mtxOpaquePipelines) {
                        for (const auto& [materialMacros, pPipeline] : pipelines.shaderPipelines) {
                            const auto it = materialMacros.find(ShaderMacro::PS_USE_MATERIAL_TRANSPARENCY);
                            REQUIRE(it == materialMacros.end());
                        }
                    }
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

TEST_CASE("change texture while spawned") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create and setup camera.
                auto pCamera = std::make_shared<TransientCamera>();
                pCamera->setLocation(glm::vec3(-1.0F, 0.0F, 0.0F));

                // Make it active.
                getCameraManager()->setActiveCamera(pCamera);

                // Prepare textures.
                if (!prepareDiffuseTextures()) {
                    REQUIRE(false);
                }

                // Spawn sample mesh.
                pMeshNode = gc_new<MeshNode>();
                auto mtxMeshData = pMeshNode->getMeshData();
                {
                    std::scoped_lock guard(*mtxMeshData.first);
                    (*mtxMeshData.second) = PrimitiveMeshGenerator::createCube(1.0F);
                }

                // Set texture before spawning.
                pMeshNode->getMaterial()->setDiffuseTexture(sImportedTexture1PathRelativeRes);

                getWorldRootNode()->addChildNode(pMeshNode);
                pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

                // Make sure there's only 1 texture in memory.
                REQUIRE(
                    getWindow()
                        ->getRenderer()
                        ->getResourceManager()
                        ->getTextureManager()
                        ->getTextureInMemoryCount() == 1);

                iFramesSpentWaiting = 0;
            });
        }
        virtual void onBeforeNewFrame(float delta) override {
            iFramesSpentWaiting += 1;

            if (iFramesSpentWaiting >= iFramesToWait) {
                // Make sure something was rendered (in case we forgot the camera).
                REQUIRE(getWindow()->getRenderer()->getLastFrameDrawCallCount() > 0);

                if (!bChangedTexture) {
                    // Change texture.
                    pMeshNode->getMaterial()->setDiffuseTexture(sImportedTexture2PathRelativeRes);

                    // Now wait for a few frames to be drawn.
                    iFramesSpentWaiting = 0;
                    bChangedTexture = true;
                    return;
                }

                getWindow()->close();
            }
        }
        virtual ~TestGameInstance() override {}

    private:
        gc<MeshNode> pMeshNode;
        bool bChangedTexture = false;
        size_t iFramesSpentWaiting = 0;
        const size_t iFramesToWait = 10;
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

TEST_CASE("serialize and deserialize a node tree with materials") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Prepare textures.
                if (!prepareDiffuseTextures()) {
                    REQUIRE(false);
                }

                // Prepare parent mesh.
                const auto pMeshNodeParent = gc_new<MeshNode>();
                auto mtxMeshData = pMeshNodeParent->getMeshData();
                {
                    std::scoped_lock guard(*mtxMeshData.first);
                    (*mtxMeshData.second) = PrimitiveMeshGenerator::createCube(1.0F);
                }

                // Set texture before spawning.
                pMeshNodeParent->getMaterial()->setDiffuseTexture(sImportedTexture1PathRelativeRes);

                // Spawn parent mesh.
                getWorldRootNode()->addChildNode(pMeshNodeParent);
                pMeshNodeParent->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

                // Prepare child mesh.
                const auto pMeshNodeChild = gc_new<MeshNode>();
                mtxMeshData = pMeshNodeChild->getMeshData();
                {
                    std::scoped_lock guard(*mtxMeshData.first);
                    (*mtxMeshData.second) = PrimitiveMeshGenerator::createCube(1.0F);
                }

                // Set texture before spawning.
                pMeshNodeChild->getMaterial()->setDiffuseTexture(sImportedTexture2PathRelativeRes);

                // Spawn child mesh.
                pMeshNodeParent->addChildNode(pMeshNodeChild);
                pMeshNodeChild->setWorldLocation(glm::vec3(1.0F, 3.0F, 0.0F));

                // Make sure there's only 2 textures in memory.
                REQUIRE(
                    getWindow()
                        ->getRenderer()
                        ->getResourceManager()
                        ->getTextureManager()
                        ->getTextureInMemoryCount() == 2);

                // Prepare path to node tree.
                const auto pathToNodeTree = ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                                            "test" / "temp" / "materialNodeTree1";

                // Serialize tree.
                auto optionalError = pMeshNodeParent->serializeNodeTree(pathToNodeTree, false);
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create a new world.
                createWorld([this, pathToNodeTree](const std::optional<Error>& optionalWorldError1) {
                    if (optionalWorldError1.has_value()) {
                        auto error = optionalWorldError1.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }

                    // Deserialize node tree.
                    auto result = Node::deserializeNodeTree(pathToNodeTree);
                    if (std::holds_alternative<Error>(result)) {
                        auto error = std::get<Error>(std::move(result));
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    auto pDeserialized = std::get<gc<Node>>(std::move(result));
                    const auto pMeshNodeParent = gc_dynamic_pointer_cast<MeshNode>(pDeserialized);
                    REQUIRE(pMeshNodeParent != nullptr);

                    // Get child node.
                    REQUIRE(pMeshNodeParent->getChildNodes()->second->size() == 1);
                    const auto pMeshNodeChild =
                        gc_dynamic_pointer_cast<MeshNode>(pMeshNodeParent->getChildNodes()->second->at(0));
                    REQUIRE(pMeshNodeChild != nullptr);

                    // Make sure there are no textures in memory.
                    REQUIRE(
                        getWindow()
                            ->getRenderer()
                            ->getResourceManager()
                            ->getTextureManager()
                            ->getTextureInMemoryCount() == 0);

                    // Make sure texture paths are correct.
                    REQUIRE(
                        pMeshNodeParent->getMaterial()->getPathToDiffuseTextureResource() ==
                        sImportedTexture1PathRelativeRes);
                    REQUIRE(
                        pMeshNodeChild->getMaterial()->getPathToDiffuseTextureResource() ==
                        sImportedTexture2PathRelativeRes);

                    // Spawn nodes.
                    getWorldRootNode()->addChildNode(pMeshNodeParent);

                    // Make sure there are 2 textures in memory.
                    const auto pRenderer = getWindow()->getRenderer();
                    REQUIRE(
                        pRenderer->getResourceManager()->getTextureManager()->getTextureInMemoryCount() == 2);

                    // Make sure there's only 2 pipelines (1 opaque + 1 depth only).
                    REQUIRE(pRenderer->getPipelineManager()->getCurrentGraphicsPipelineCount() == 2);

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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
}

TEST_CASE("changing diffuse texture from non-main thread should not cause deadlock") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create and setup camera.
                auto pCamera = std::make_shared<TransientCamera>();
                pCamera->setLocation(glm::vec3(-1.0F, 0.0F, 0.0F));

                // Make it active.
                getCameraManager()->setActiveCamera(pCamera);

                // Prepare textures.
                if (!prepareDiffuseTextures()) {
                    REQUIRE(false);
                }

                // Spawn sample mesh.
                const auto pMeshNode = gc_new<MeshNode>();
                pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));

                // Set texture before spawning.
                pMeshNode->getMaterial()->setDiffuseTexture(sImportedTexture1PathRelativeRes);

                getWorldRootNode()->addChildNode(pMeshNode);
                pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

                addTaskToThreadPool([this, pRawMeshNode = &*pMeshNode]() {
                    const auto iFramesBefore = iFramesRendered;

                    do {
                        Logger::get().info("attempting to test a deadlock, waiting for frame");
                        constexpr size_t iTryCont = 1000;
                        for (size_t i = 0; i < iTryCont; i++) {
                            if (i % 2 == 0) {
                                pRawMeshNode->getMaterial()->setDiffuseTexture(
                                    sImportedTexture2PathRelativeRes);
                            } else {
                                pRawMeshNode->getMaterial()->setDiffuseTexture(
                                    sImportedTexture1PathRelativeRes);
                            }

                            if (i % (iTryCont / 10) == 0) {
                                Logger::get().info(std::format(
                                    "testing deadlock: {}%",
                                    static_cast<size_t>(
                                        static_cast<float>(i) / static_cast<float>(iTryCont) * 100.0F)));
                            }
                        }

                    } while (iFramesRendered <= iFramesBefore + 1);

                    Logger::get().info("finished testing a deadlock");

                    getWindow()->close();
                });
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float delta) override { iFramesRendered += 1; }

    private:
        size_t iFramesRendered = 0;
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

TEST_CASE("using 1 texture in 2 material has only 1 texture in memory") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([this](const std::optional<Error>& optionalError) {
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Prepare textures.
                if (!prepareDiffuseTextures()) {
                    REQUIRE(false);
                }

                {
                    // Spawn sample mesh.
                    const auto pMeshNode = gc_new<MeshNode>();
                    auto mtxMeshData = pMeshNode->getMeshData();
                    {
                        std::scoped_lock guard(*mtxMeshData.first);
                        (*mtxMeshData.second) = PrimitiveMeshGenerator::createCube(1.0F);
                    }

                    // Set texture before spawning.
                    pMeshNode->getMaterial()->setDiffuseTexture(sImportedTexture1PathRelativeRes);

                    getWorldRootNode()->addChildNode(pMeshNode);
                    pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));
                }

                {
                    // Spawn sample mesh.
                    const auto pMeshNode = gc_new<MeshNode>();
                    auto mtxMeshData = pMeshNode->getMeshData();
                    {
                        std::scoped_lock guard(*mtxMeshData.first);
                        (*mtxMeshData.second) = PrimitiveMeshGenerator::createCube(1.0F);
                    }

                    // Set texture before spawning.
                    // Use the same texture.
                    pMeshNode->getMaterial()->setDiffuseTexture(sImportedTexture1PathRelativeRes);

                    getWorldRootNode()->addChildNode(pMeshNode);
                    pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));
                }

                // Make sure there's only 1 texture in memory.
                REQUIRE(
                    getWindow()
                        ->getRenderer()
                        ->getResourceManager()
                        ->getTextureManager()
                        ->getTextureInMemoryCount() == 1);

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
