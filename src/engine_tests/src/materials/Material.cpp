// Custom.
#include "materials/Material.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/nodes/MeshNode.h"
#include "misc/ProjectPaths.h"
#include "materials/EngineShaderNames.hpp"
#include "materials/Shader.h"

// External.
#include "catch2/catch_test_macros.hpp"

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
                    EngineShaderNames::sMeshNodeVertexShaderName,
                    EngineShaderNames::sMeshNodePixelShaderName,
                    false);
                if (std::holds_alternative<Error>(resultOpaque)) {
                    Error error = std::get<Error>(std::move(resultOpaque));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                auto resultTransparent = Material::create(
                    EngineShaderNames::sMeshNodeVertexShaderName,
                    EngineShaderNames::sMeshNodePixelShaderName,
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
                meshData.getIndices()->push_back(0);

                // Create nodes.
                auto pMeshNodeTransparent = gc_new<MeshNode>("Transparent material node");
                pMeshNodeTransparent->setMaterial(
                    std::get<std::shared_ptr<Material>>(std::move(resultTransparent)));
                pMeshNodeTransparent->setMeshData(meshData);

                auto pMeshNodeOpaque = gc_new<MeshNode>("Opaque material node");
                pMeshNodeOpaque->setMaterial(std::get<std::shared_ptr<Material>>(std::move(resultOpaque)));
                pMeshNodeOpaque->setMeshData(meshData);

                // There should be no PSOs created since no material is spawned.
                REQUIRE(getWindow()->getRenderer()->getPsoManager()->getCreatedGraphicsPsoCount() == 0);

                // Spawn.
                getWorldRootNode()->addChildNode(pMeshNodeOpaque);
                getWorldRootNode()->addChildNode(pMeshNodeTransparent);

                // Check everything.
                REQUIRE(Material::getCurrentMaterialCount() == 2);
                REQUIRE(getWindow()->getRenderer()->getPsoManager()->getCreatedGraphicsPsoCount() == 2);

                // Despawn one node.
                pMeshNodeOpaque->detachFromParentAndDespawn();
                REQUIRE(Material::getCurrentMaterialCount() == 2);
                REQUIRE(getWindow()->getRenderer()->getPsoManager()->getCreatedGraphicsPsoCount() == 1);

                // Despawn all nodes.
                createWorld([&](const std::optional<Error>& optionalError) {
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    // Check that everything is cleaned up.
                    REQUIRE(Material::getCurrentMaterialCount() == 0);
                    REQUIRE(getWindow()->getRenderer()->getPsoManager()->getCreatedGraphicsPsoCount() == 0);

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
    REQUIRE(Material::getCurrentMaterialCount() == 0);
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

                {
                    // Create material.
                    auto result = Material::create(
                        EngineShaderNames::sMeshNodeVertexShaderName,
                        EngineShaderNames::sMeshNodePixelShaderName,
                        true,
                        "My Material");
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }

                    // Serialize.
                    const auto pMaterial = std::get<std::shared_ptr<Material>>(std::move(result));
                    auto optionalError = pMaterial->serialize(pathToFileInTemp, false);
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                REQUIRE(Material::getCurrentMaterialCount() == 0);

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
                }

                REQUIRE(Material::getCurrentMaterialCount() == 0);

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
    REQUIRE(Material::getCurrentMaterialCount() == 0);
}

TEST_CASE("unused materials unload shaders from memory") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            std::vector<ShaderDescription> vShadersToCompile = {
                ShaderDescription(
                    "test.custom_mesh_node.vs",
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                        "test/shaders/hlsl/CustomMeshNode.hlsl",
                    ShaderType::VERTEX_SHADER,
                    "vsCustomMeshNode",
                    {}),
                ShaderDescription(
                    "test.custom_mesh_node.ps",
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                        "test/shaders/hlsl/CustomMeshNode.hlsl",
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
                        meshData.getIndices()->push_back(0);

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
    REQUIRE(Material::getCurrentMaterialCount() == 0);
}
