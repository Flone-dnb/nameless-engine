// Custom.
#include "game/nodes/MeshNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "materials/Material.h"
#include "materials/EngineShaderNames.hpp"
#include "../../io/ReflectionTest.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("serialize and deserialize MeshNode") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                const std::filesystem::path pathToFileInTemp =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                    "TESTING_MeshNodeSerialization_TESTING.toml";

                // Create mesh data.
                MeshVertex vertex1, vertex2;
                vertex1.position = glm::vec3(5123.91827f, -12225.24142f, -5.0f);
                vertex1.normal = glm::vec3(10.0f, -1111.22212f, 0.0f);
                vertex1.uv = glm::vec3(10.0f, -8885.14122f, 0.0f);
                vertex2.position = glm::vec3(-1.0f, -2.0f, -3.0f);
                vertex2.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
                vertex2.uv = glm::vec2(-1.0f, -2.0f);

                {
                    // Create material.
                    auto result = Material::create(
                        EngineShaderNames::sMeshNodeVertexShaderName,
                        EngineShaderNames::sMeshNodePixelShaderName,
                        true,
                        "My Material");
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }

                    MeshData meshData;
                    meshData.getVertices()->push_back(vertex1);
                    meshData.getVertices()->push_back(vertex2);
                    meshData.getIndices()->push_back(0);
                    meshData.getIndices()->push_back(1);

                    // Create node and initialize.
                    const auto pMeshNode = gc_new<MeshNode>("My cool node");
                    pMeshNode->setMaterial(std::get<std::shared_ptr<Material>>(std::move(result)));
                    pMeshNode->setMeshData(std::move(meshData));

                    // Serialize node.
                    auto optionalError = pMeshNode->serialize(pathToFileInTemp, true); // use backup file
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                gc_collector()->collect();
                REQUIRE(Material::getCurrentMaterialCount() == 0);

                const auto pathToExternalFile =
                    pathToFileInTemp.parent_path() / (pathToFileInTemp.stem().string() + ".0.meshData.toml");
                REQUIRE(std::filesystem::exists(pathToExternalFile));

                // Delete original external file.
                std::filesystem::remove(pathToExternalFile);

                {
                    // Deserialize.
                    auto result = Serializable::deserialize<gc, MeshNode>(pathToFileInTemp);
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    const auto pMeshNode = std::get<gc<MeshNode>>(std::move(result));

                    // Original file should be restored.
                    REQUIRE(std::filesystem::exists(pathToExternalFile));

                    // Check.
                    REQUIRE(pMeshNode->getNodeName() == "My cool node");
                    REQUIRE(pMeshNode->getMaterial()->isUsingTransparency());
                    REQUIRE(pMeshNode->getMaterial()->getMaterialName() == "My Material");
                    const auto mtxMeshData = pMeshNode->getMeshData();
                    std::scoped_lock guard(*mtxMeshData.first);
                    REQUIRE(mtxMeshData.second->getVertices()->size() == 2);
                    REQUIRE(mtxMeshData.second->getIndices()->size() == 2);
                    REQUIRE(mtxMeshData.second->getVertices()->at(0) == vertex1);
                    REQUIRE(mtxMeshData.second->getVertices()->at(1) == vertex2);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0) == 0);
                    REQUIRE(mtxMeshData.second->getIndices()->at(1) == 1);
                }

                gc_collector()->collect();
                REQUIRE(Material::getCurrentMaterialCount() == 0);

                // Cleanup.
                if (std::filesystem::exists(pathToFileInTemp)) {
                    ConfigManager::removeFile(pathToFileInTemp);
                }
                if (std::filesystem::exists(pathToExternalFile)) {
                    ConfigManager::removeFile(pathToExternalFile);
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentMaterialCount() == 0);
}

TEST_CASE("serialize and deserialize array of mesh vertices") {
    using namespace ne;

    const std::filesystem::path pathToFileInTemp =
        ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
        "TESTING_MeshVerticesSerialization_TESTING.toml";

    MeshVertices vertices;

    MeshVertex vertex1, vertex2;
    vertex1.position = glm::vec3(5123.91827f, -12225.24142f, -5.0f);
    vertex1.normal = glm::vec3(10.0f, -1111.22212f, 0.0f);
    vertex1.uv = glm::vec3(10.0f, -8885.14122f, 0.0f);
    vertex2.position = glm::vec3(-1.0f, -2.0f, -3.0f);
    vertex2.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
    vertex2.uv = glm::vec2(-1.0f, -2.0f);

    vertices.vVertices.push_back(vertex1);
    vertices.vVertices.push_back(vertex2);

    // Serialize.
    auto optionalError = vertices.serialize(pathToFileInTemp, false);
    if (optionalError.has_value()) {
        auto error = optionalError.value();
        error.addEntry();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    // Deserialize.
    auto result = Serializable::deserialize<std::shared_ptr, MeshVertices>(pathToFileInTemp);
    if (std::holds_alternative<Error>(result)) {
        auto error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }
    const auto pMeshVertices = std::get<std::shared_ptr<MeshVertices>>(std::move(result));

    // Check.
    REQUIRE(pMeshVertices->vVertices == vertices.vVertices);
    REQUIRE(pMeshVertices->vVertices.size() == vertices.vVertices.size());
    REQUIRE(pMeshVertices->vVertices[0].position == vertices.vVertices[0].position);
    REQUIRE(pMeshVertices->vVertices[0].uv == vertices.vVertices[0].uv);
    REQUIRE(pMeshVertices->vVertices[1].position == vertices.vVertices[1].position);
    REQUIRE(pMeshVertices->vVertices[1].uv == vertices.vVertices[1].uv);

    // Cleanup.
    if (std::filesystem::exists(pathToFileInTemp)) {
        std::filesystem::remove(pathToFileInTemp);
    }
}

TEST_CASE("serialize and deserialize MeshNode as part of a node tree") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                const std::filesystem::path pathToFileInTemp =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                    "TESTING_MeshNodeTreeSerializationWithoutOriginalObject_TESTING.toml";

                // Create mesh data.
                MeshVertex vertex1, vertex2;
                vertex1.position = glm::vec3(5123.91827f, -12225.24142f, -5.0f);
                vertex1.normal = glm::vec3(10.0f, -1111.22212f, 0.0f);
                vertex1.uv = glm::vec3(10.0f, -8885.14122f, 0.0f);
                vertex2.position = glm::vec3(-1.0f, -2.0f, -3.0f);
                vertex2.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
                vertex2.uv = glm::vec2(-1.0f, -2.0f);

                {
                    // Create material.
                    auto result = Material::create(
                        EngineShaderNames::sMeshNodeVertexShaderName,
                        EngineShaderNames::sMeshNodePixelShaderName,
                        true,
                        "My Material");
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }

                    MeshData meshData;
                    meshData.getVertices()->push_back(vertex1);
                    meshData.getVertices()->push_back(vertex2);
                    meshData.getIndices()->push_back(0);
                    meshData.getIndices()->push_back(1);

                    // Create node and initialize.
                    const auto pMeshNode = gc_new<MeshNode>("My cool node");
                    pMeshNode->setMaterial(std::get<std::shared_ptr<Material>>(std::move(result)));
                    pMeshNode->setMeshData(std::move(meshData));
                    getWorldRootNode()->addChildNode(
                        pMeshNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

                    // Serialize tree.
                    auto optionalError =
                        getWorldRootNode()->serializeNodeTree(pathToFileInTemp, true); // use backup file
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                gc_collector()->collect();

                const auto pathToExternalFile =
                    pathToFileInTemp.parent_path() / (pathToFileInTemp.stem().string() + ".1.meshData.toml");
                REQUIRE(std::filesystem::exists(pathToExternalFile));

                // Delete original external file.
                std::filesystem::remove(pathToExternalFile);

                {
                    // Deserialize.
                    auto result = Node::deserializeNodeTree(pathToFileInTemp);
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    const auto pRootNode = std::get<gc<Node>>(std::move(result));

                    REQUIRE(pRootNode->getChildNodes()->second->size() == 1);
                    const auto pMeshNode =
                        gc_dynamic_pointer_cast<MeshNode>(pRootNode->getChildNodes()->second->operator[](0));

                    // Original file should be restored.
                    REQUIRE(std::filesystem::exists(pathToExternalFile));

                    // Check.
                    REQUIRE(pMeshNode->getNodeName() == "My cool node");
                    REQUIRE(pMeshNode->getMaterial()->isUsingTransparency());
                    REQUIRE(pMeshNode->getMaterial()->getMaterialName() == "My Material");
                    const auto mtxMeshData = pMeshNode->getMeshData();
                    std::scoped_lock guard(*mtxMeshData.first);
                    REQUIRE(mtxMeshData.second->getVertices()->size() == 2);
                    REQUIRE(mtxMeshData.second->getIndices()->size() == 2);
                    REQUIRE(mtxMeshData.second->getVertices()->at(0) == vertex1);
                    REQUIRE(mtxMeshData.second->getVertices()->at(1) == vertex2);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0) == 0);
                    REQUIRE(mtxMeshData.second->getIndices()->at(1) == 1);
                }

                gc_collector()->collect();

                // Cleanup.
                if (std::filesystem::exists(pathToFileInTemp)) {
                    ConfigManager::removeFile(pathToFileInTemp);
                }
                if (std::filesystem::exists(pathToExternalFile)) {
                    ConfigManager::removeFile(pathToExternalFile);
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentMaterialCount() == 0);
}

TEST_CASE("serialize and deserialize MeshNode as part of a node tree with original object") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                const std::filesystem::path pathToNodeFile =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                    "TESTING_MeshNodeOriginalObjectSerialization_TESTING.toml";

                const std::filesystem::path pathToFileInTemp =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                    "TESTING_MeshNodeTreeSerialization_TESTING.toml";

                // Create mesh data.
                MeshVertex vertex1, vertex2;
                vertex1.position = glm::vec3(5123.91827f, -12225.24142f, -5.0f);
                vertex1.normal = glm::vec3(10.0f, -1111.22212f, 0.0f);
                vertex1.uv = glm::vec3(10.0f, -8885.14122f, 0.0f);
                vertex2.position = glm::vec3(-1.0f, -2.0f, -3.0f);
                vertex2.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
                vertex2.uv = glm::vec2(-1.0f, -2.0f);

                {
                    // Create material.
                    auto result = Material::create(
                        EngineShaderNames::sMeshNodeVertexShaderName,
                        EngineShaderNames::sMeshNodePixelShaderName,
                        true,
                        "My Material");
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }

                    MeshData meshData;
                    meshData.getVertices()->push_back(vertex1);
                    meshData.getVertices()->push_back(vertex2);
                    meshData.getIndices()->push_back(0);
                    meshData.getIndices()->push_back(1);

                    // Create node and initialize.
                    const auto pMeshNode = gc_new<MeshNode>("My cool node");
                    pMeshNode->setMaterial(std::get<std::shared_ptr<Material>>(std::move(result)));
                    pMeshNode->setMeshData(std::move(meshData));

                    // Serialize node.
                    auto optionalError = pMeshNode->serialize(pathToNodeFile, true); // use backup file
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                {
                    // Deserialize mesh node.
                    auto result = Serializable::deserialize<gc, MeshNode>(pathToNodeFile);
                    if (std::holds_alternative<Error>(result)) {
                        auto error = std::get<Error>(result);
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    auto pMeshNode = std::get<gc<MeshNode>>(std::move(result));

                    getWorldRootNode()->addChildNode(
                        pMeshNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

                    // Serialize tree.
                    auto optionalError =
                        getWorldRootNode()->serializeNodeTree(pathToFileInTemp, true); // use backup file
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                // tree should not create external file
                const auto pathToExternalTreeFile =
                    pathToFileInTemp.parent_path() / (pathToFileInTemp.stem().string() + ".1.meshData.toml");
                REQUIRE(!std::filesystem::exists(pathToExternalTreeFile));

                // node should have external file
                const auto pathToExternalFile =
                    pathToNodeFile.parent_path() / (pathToNodeFile.stem().string() + ".0.meshData.toml");
                REQUIRE(std::filesystem::exists(pathToExternalFile));

                MeshVertex vertex3;
                vertex3.position = glm::vec3(-1.0f, -2.0f, -3.0f);
                vertex3.uv = glm::vec2(-1.0f, -2.0f);

                {
                    // Modify mesh data.
                    auto pMeshNode = gc_dynamic_pointer_cast<MeshNode>(
                        getWorldRootNode()->getChildNodes()->second->operator[](0));

                    MeshData meshData;
                    meshData.getVertices()->push_back(vertex1);
                    meshData.getVertices()->push_back(vertex2);
                    meshData.getVertices()->push_back(vertex3);
                    meshData.getIndices()->push_back(0);
                    meshData.getIndices()->push_back(1);
                    meshData.getIndices()->push_back(2);

                    pMeshNode->setMeshData(meshData);

                    // Serialize tree again.
                    auto optionalError =
                        getWorldRootNode()->serializeNodeTree(pathToFileInTemp, true); // use backup file
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                // now external file for tree should exist because we modified the node
                REQUIRE(std::filesystem::exists(pathToExternalTreeFile));
                // original external file should still exist
                REQUIRE(std::filesystem::exists(pathToExternalFile));

                // Delete original external file.
                std::filesystem::remove(pathToExternalFile);

                {
                    // Deserialize.
                    auto result = Node::deserializeNodeTree(pathToFileInTemp);
                    if (std::holds_alternative<Error>(result)) {
                        Error error = std::get<Error>(std::move(result));
                        error.addEntry();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    const auto pRootNode = std::get<gc<Node>>(std::move(result));

                    REQUIRE(pRootNode->getChildNodes()->second->size() == 1);
                    const auto pMeshNode =
                        gc_dynamic_pointer_cast<MeshNode>(pRootNode->getChildNodes()->second->operator[](0));

                    // Original file should be restored.
                    REQUIRE(std::filesystem::exists(pathToExternalFile));

                    // Check.
                    REQUIRE(pMeshNode->getNodeName() == "My cool node");
                    REQUIRE(pMeshNode->getMaterial()->isUsingTransparency());
                    REQUIRE(pMeshNode->getMaterial()->getMaterialName() == "My Material");
                    const auto mtxMeshData = pMeshNode->getMeshData();
                    std::scoped_lock guard(*mtxMeshData.first);
                    REQUIRE(mtxMeshData.second->getVertices()->size() == 3);
                    REQUIRE(mtxMeshData.second->getIndices()->size() == 3);
                    REQUIRE(mtxMeshData.second->getVertices()->at(0) == vertex1);
                    REQUIRE(mtxMeshData.second->getVertices()->at(1) == vertex2);
                    REQUIRE(mtxMeshData.second->getVertices()->at(2) == vertex3);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0) == 0);
                    REQUIRE(mtxMeshData.second->getIndices()->at(1) == 1);
                    REQUIRE(mtxMeshData.second->getIndices()->at(2) == 2);
                }

                gc_collector()->collect();

                // Cleanup.
                if (std::filesystem::exists(pathToFileInTemp)) {
                    ConfigManager::removeFile(pathToFileInTemp);
                }
                if (std::filesystem::exists(pathToExternalFile)) {
                    ConfigManager::removeFile(pathToExternalFile);
                }
                if (std::filesystem::exists(pathToNodeFile)) {
                    ConfigManager::removeFile(pathToNodeFile);
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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentMaterialCount() == 0);
}

TEST_CASE("MeshNode's meshdata deserialization backwards compatibility") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                const std::filesystem::path pathToFileInTemp =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "meshnode" /
                    "MeshNodeSerializationTestForBackwardsCompatibility.toml";

                // Create mesh data.
                MeshVertex vertex1, vertex2;
                vertex1.position = glm::vec3(5123.91827f, -12225.24142f, -5.0f);
                vertex1.normal = glm::vec3(10.0f, -1111.22212f, 0.0f);
                vertex1.uv = glm::vec3(10.0f, -8885.14122f, 0.0f);
                vertex2.position = glm::vec3(-1.0f, -2.0f, -3.0f);
                vertex2.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
                vertex2.uv = glm::vec2(-1.0f, -2.0f);

                // Deserialize.
                auto result = Serializable::deserialize<gc, MeshNode>(pathToFileInTemp);
                if (std::holds_alternative<Error>(result)) {
                    Error error = std::get<Error>(std::move(result));
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                const auto pMeshNode = std::get<gc<MeshNode>>(std::move(result));

                // Check.
                const auto mtxMeshData = pMeshNode->getMeshData();
                std::scoped_lock guard(*mtxMeshData.first);
                REQUIRE(mtxMeshData.second->getVertices()->size() == 2);
                REQUIRE(mtxMeshData.second->getIndices()->size() == 2);
                REQUIRE(mtxMeshData.second->getVertices()->at(0) == vertex1);
                REQUIRE(mtxMeshData.second->getVertices()->at(1) == vertex2);
                REQUIRE(mtxMeshData.second->getIndices()->at(0) == 0);
                REQUIRE(mtxMeshData.second->getIndices()->at(1) == 1);

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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentMaterialCount() == 0);
}

TEST_CASE("shader read/write resources exist only when MeshNode is spawned") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld([&](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) {
                    auto error = optionalWorldError.value();
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create dummy mesh data.
                MeshVertex vertex1;
                vertex1.position = glm::vec3(5123.91827f, -12225.24142f, -5.0f);
                vertex1.normal = glm::vec3(10.0f, -1111.22212f, 0.0f);
                vertex1.uv = glm::vec3(10.0f, -8885.14122f, 0.0f);

                // Create material.
                auto result = Material::create(
                    EngineShaderNames::sMeshNodeVertexShaderName,
                    EngineShaderNames::sMeshNodePixelShaderName,
                    false,
                    "My Material");
                if (std::holds_alternative<Error>(result)) {
                    Error error = std::get<Error>(std::move(result));
                    error.addEntry();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create sample mesh data.
                MeshData meshData;
                constexpr size_t iSize = 1000000;
                meshData.getVertices()->resize(iSize);
                meshData.getIndices()->resize(iSize);
                for (size_t i = 0; i < iSize; i++) {
                    meshData.getVertices()->at(i) = vertex1;
                    meshData.getIndices()->at(i) = static_cast<unsigned int>(i);
                }

                // Create node and initialize.
                const auto pMeshNode = gc_new<MeshNode>("My cool node");
                pMeshNode->setMaterial(std::get<std::shared_ptr<Material>>(std::move(result)));
                pMeshNode->setMeshData(std::move(meshData));

                // Get shader resource manager.
                const auto pShaderRwResourceManager =
                    getWindow()->getRenderer()->getShaderCpuReadWriteResourceManager();
                auto pMtxResources = pShaderRwResourceManager->getResources();

                // Make sure no shader read/write resources created.
                std::scoped_lock shaderRwResourcesGuard(pMtxResources->first);
                REQUIRE(pMtxResources->second.vAll.empty());
                REQUIRE(pMtxResources->second.toBeUpdated.empty());

                // Save VRAM to check later.
                const auto iVramNotSpawned = getWindow()->getRenderer()->getUsedVideoMemoryInMb();

                // Spawn mesh node.
                getWorldRootNode()->addChildNode(
                    pMeshNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

                // Make sure there are 2 resources (meshData and materialData).
                REQUIRE(pMtxResources->second.vAll.size() == 2);
                REQUIRE(pMtxResources->second.toBeUpdated.size() == 2);

                // Check VRAM.
                const auto iVramSpawned = getWindow()->getRenderer()->getUsedVideoMemoryInMb();
                REQUIRE(iVramSpawned > iVramNotSpawned);

                // Despawn mesh node.
                pMeshNode->detachFromParentAndDespawn();

                // Make sure resources were freed.
                REQUIRE(pMtxResources->second.vAll.empty());
                REQUIRE(pMtxResources->second.toBeUpdated.empty());

                // Check VRAM.
                const auto iVramDespawned = getWindow()->getRenderer()->getUsedVideoMemoryInMb();
                REQUIRE(iVramDespawned < iVramSpawned / 2);

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

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentMaterialCount() == 0);
}
