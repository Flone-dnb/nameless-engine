// Custom.
#include "game/nodes/MeshNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "materials/Material.h"
#include "materials/EngineShaderNames.hpp"
#include "../../io/ReflectionTest.h"
#include "misc/PrimitiveMeshGenerator.h"

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
                    error.addCurrentLocationToErrorStack();
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

                    MeshData meshData;
                    meshData.getVertices()->push_back(vertex1);
                    meshData.getVertices()->push_back(vertex2);
                    meshData.getIndices()->push_back({0, 1});

                    // Create node and initialize.
                    const auto pMeshNode = gc_new<MeshNode>("My cool node");
                    pMeshNode->setMaterial(std::get<std::shared_ptr<Material>>(std::move(result)));
                    pMeshNode->setMeshData(std::move(meshData));

                    // Serialize node.
                    auto optionalError = pMeshNode->serialize(pathToFileInTemp, true); // use backup file
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                gc_collector()->collect();
                REQUIRE(Material::getCurrentAliveMaterialCount() == 0);

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
                        error.addCurrentLocationToErrorStack();
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
                    REQUIRE(mtxMeshData.second->getIndices()->size() == 1);
                    REQUIRE(mtxMeshData.second->getVertices()->at(0) == vertex1);
                    REQUIRE(mtxMeshData.second->getVertices()->at(1) == vertex2);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0).size() == 2);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0)[0] == 0);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0)[1] == 1);
                }

                gc_collector()->collect();
                REQUIRE(Material::getCurrentAliveMaterialCount() == 0);

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
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
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
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    // Deserialize.
    auto result = Serializable::deserialize<std::shared_ptr, MeshVertices>(pathToFileInTemp);
    if (std::holds_alternative<Error>(result)) {
        auto error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
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
                    error.addCurrentLocationToErrorStack();
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

                    MeshData meshData;
                    meshData.getVertices()->push_back(vertex1);
                    meshData.getVertices()->push_back(vertex2);
                    meshData.getIndices()->push_back({0, 1});

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
                        error.addCurrentLocationToErrorStack();
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
                        error.addCurrentLocationToErrorStack();
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
                    REQUIRE(mtxMeshData.second->getIndices()->size() == 1);
                    REQUIRE(mtxMeshData.second->getVertices()->at(0) == vertex1);
                    REQUIRE(mtxMeshData.second->getVertices()->at(1) == vertex2);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0).size() == 2);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0)[0] == 0);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0)[1] == 1);
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
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
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
                    error.addCurrentLocationToErrorStack();
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

                    MeshData meshData;
                    meshData.getVertices()->push_back(vertex1);
                    meshData.getVertices()->push_back(vertex2);
                    meshData.getIndices()->push_back({0, 1});

                    // Create node and initialize.
                    const auto pMeshNode = gc_new<MeshNode>("My cool node");
                    pMeshNode->setMaterial(std::get<std::shared_ptr<Material>>(std::move(result)));
                    pMeshNode->setMeshData(std::move(meshData));

                    // Serialize node.
                    auto optionalError = pMeshNode->serialize(pathToNodeFile, true); // use backup file
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                {
                    // Deserialize mesh node.
                    auto result = Serializable::deserialize<gc, MeshNode>(pathToNodeFile);
                    if (std::holds_alternative<Error>(result)) {
                        auto error = std::get<Error>(result);
                        error.addCurrentLocationToErrorStack();
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
                        error.addCurrentLocationToErrorStack();
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
                    meshData.getIndices()->push_back({0, 1, 2});

                    pMeshNode->setMeshData(meshData);

                    // Serialize tree again.
                    auto optionalError =
                        getWorldRootNode()->serializeNodeTree(pathToFileInTemp, true); // use backup file
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
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
                        error.addCurrentLocationToErrorStack();
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
                    REQUIRE(mtxMeshData.second->getIndices()->size() == 1);
                    REQUIRE(mtxMeshData.second->getVertices()->at(0) == vertex1);
                    REQUIRE(mtxMeshData.second->getVertices()->at(1) == vertex2);
                    REQUIRE(mtxMeshData.second->getVertices()->at(2) == vertex3);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0).size() == 3);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0)[0] == 0);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0)[1] == 1);
                    REQUIRE(mtxMeshData.second->getIndices()->at(0)[2] == 2);
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
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
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
                    error.addCurrentLocationToErrorStack();
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
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                const auto pMeshNode = std::get<gc<MeshNode>>(std::move(result));

                // Check.
                const auto mtxMeshData = pMeshNode->getMeshData();
                std::scoped_lock guard(*mtxMeshData.first);
                REQUIRE(mtxMeshData.second->getVertices()->size() == 2);
                REQUIRE(mtxMeshData.second->getIndices()->size() == 1);
                REQUIRE(mtxMeshData.second->getVertices()->at(0) == vertex1);
                REQUIRE(mtxMeshData.second->getVertices()->at(1) == vertex2);
                REQUIRE(mtxMeshData.second->getIndices()->at(0).size() == 2);
                REQUIRE(mtxMeshData.second->getIndices()->at(0)[0] == 0);
                REQUIRE(mtxMeshData.second->getIndices()->at(0)[1] == 1);

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
                    error.addCurrentLocationToErrorStack();
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
                    EngineShaderNames::MeshNode::sVertexShaderName,
                    EngineShaderNames::MeshNode::sPixelShaderName,
                    false,
                    "My Material");
                if (std::holds_alternative<Error>(result)) {
                    Error error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create sample mesh data.
                MeshData meshData;
                constexpr size_t iSize = 5000000;
                meshData.getVertices()->resize(iSize);
                meshData.getIndices()->resize(1);
                meshData.getIndices()->at(0).resize(iSize);
                for (size_t i = 0; i < iSize; i++) {
                    meshData.getVertices()->at(i) = vertex1;
                    meshData.getIndices()->at(0).at(i) = static_cast<unsigned int>(i);
                }

                // Create node and initialize.
                const auto pMeshNode = gc_new<MeshNode>("My cool node");
                pMeshNode->setMaterial(std::get<std::shared_ptr<Material>>(std::move(result)));
                pMeshNode->setMeshData(std::move(meshData));

                // Get shader resource manager.
                const auto pShaderCpuWriteResourceManager =
                    getWindow()->getRenderer()->getShaderCpuWriteResourceManager();
                auto pMtxResources = pShaderCpuWriteResourceManager->getResources();

                // Make sure no shader read/write resources created.
                std::scoped_lock shaderCpuWriteResourcesGuard(pMtxResources->first);
                REQUIRE(pMtxResources->second.all.empty());
                for (const auto& set : pMtxResources->second.toBeUpdated) {
                    REQUIRE(set.empty());
                }

                // Save VRAM to check later.
                const auto iVramMbNotSpawned = getWindow()->getRenderer()->getUsedVideoMemoryInMb();

                // Spawn mesh node.
                getWorldRootNode()->addChildNode(
                    pMeshNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

                // Make sure there are 2 resources (meshData and materialData).
                REQUIRE(pMtxResources->second.all.size() == 2);
                for (const auto& set : pMtxResources->second.toBeUpdated) {
                    REQUIRE(set.size() == 2);
                }

                // Check VRAM.
                const auto iVramMbSpawned = getWindow()->getRenderer()->getUsedVideoMemoryInMb();
                REQUIRE(iVramMbSpawned > iVramMbNotSpawned);

                // Self check for spawned mesh size.
                constexpr size_t iMaxMeshSizeForTestMb = 512;
                if (iVramMbSpawned - iVramMbNotSpawned > iMaxMeshSizeForTestMb) {
                    Error error(fmt::format(
                        "test mesh node takes {}+ MB of VRAM, that's too much for a test, decrease "
                        "mesh vertex count",
                        iMaxMeshSizeForTestMb));
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Despawn mesh node.
                pMeshNode->detachFromParentAndDespawn();

                // Make sure resources were freed.
                REQUIRE(pMtxResources->second.all.empty());
                for (const auto& set : pMtxResources->second.toBeUpdated) {
                    REQUIRE(set.empty());
                }

                // Check VRAM.
                const auto iVramDespawned = getWindow()->getRenderer()->getUsedVideoMemoryInMb();
                REQUIRE(iVramDespawned < iVramMbSpawned / 2);

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

TEST_CASE("change spawned mesh from 2 to 1 to 3 to 3 (again) material slots") {
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

                // Spawn sample mesh.
                pMeshNode = gc_new<MeshNode>();

                auto meshData = PrimitiveMeshGenerator::createCube(1.0F);
                meshData.getIndices()->at(0) = {
                    0,  1,  2,  3,  2,  1,  // +X face.
                    8,  9,  10, 11, 10, 9,  // +Y face.
                    12, 13, 14, 15, 14, 13, // -Y face.
                    16, 17, 18, 19, 18, 17, // +Z face.
                    20, 21, 22, 23, 22, 21  // -Z face.
                };
                meshData.getIndices()->push_back({4, 5, 6, 7, 6, 5});
                pMeshNode->setMeshData(meshData);
                REQUIRE(pMeshNode->getAvailableMaterialSlotCount() == 2);

                getWorldRootNode()->addChildNode(pMeshNode);
                pMeshNode->setWorldLocation(glm::vec3(1.0F, 3.0F, 0.0F));

                pMeshNode->getMaterial(0)->setDiffuseColor(glm::vec3(1.0F, 0.0F, 0.0F));
                pMeshNode->getMaterial(1)->setDiffuseColor(glm::vec3(0.0F, 1.0F, 0.0F));

                iFrameCount = 0;

                getWindow()->close();
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float delta) override {
            iFrameCount += 1;

            if (iFrameCount == 2) {
                // Use mesh with just 1 material slot.
                pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));

                REQUIRE(pMeshNode->getAvailableMaterialSlotCount() == 1);
                REQUIRE(pMeshNode->isSpawned());
            }

            if (iFrameCount == 4) {
                // Use mesh with 3 material slots.
                auto meshData = PrimitiveMeshGenerator::createCube(1.0F);
                meshData.getIndices()->at(0) = {
                    0,  1,  2,  3,  2,  1,  // +X face.
                    12, 13, 14, 15, 14, 13, // -Y face.
                    16, 17, 18, 19, 18, 17, // +Z face.
                    20, 21, 22, 23, 22, 21  // -Z face.
                };
                meshData.getIndices()->push_back({4, 5, 6, 7, 6, 5});
                meshData.getIndices()->push_back({8, 9, 10, 11, 10, 9});
                pMeshNode->setMeshData(meshData);

                REQUIRE(pMeshNode->getAvailableMaterialSlotCount() == 3);
                REQUIRE(pMeshNode->isSpawned());

                // Enable transparency on one slot.
                pMeshNode->getMaterial(2)->setEnableTransparency(true);
            }

            if (iFrameCount == 6) {
                // Change mesh but still 3 slots.
                auto meshData = PrimitiveMeshGenerator::createCube(1.0F);
                meshData.getIndices()->at(0) = {0,  1,  2,  3,  2,  1,  12, 13, 14, 15, 14, 13,
                                                16, 17, 18, 19, 18, 17, 8,  9,  10, 11, 10, 9};
                meshData.getIndices()->push_back({4, 5, 6, 7, 6, 5});
                meshData.getIndices()->push_back({20, 21, 22, 23, 22, 21});
                pMeshNode->setMeshData(meshData);

                REQUIRE(pMeshNode->getAvailableMaterialSlotCount() == 3);
                REQUIRE(pMeshNode->isSpawned());
                REQUIRE(pMeshNode->getMaterial(2)->isTransparencyEnabled());
            }

            if (iFrameCount == 8) {
                getWindow()->close();
            }
        }

    private:
        size_t iFrameCount = 0;
        gc<MeshNode> pMeshNode;
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

TEST_CASE("check the number of pipelines on spawned mesh material slots") {
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

                const auto pPipelineManager = getWindow()->getRenderer()->getPipelineManager();

                {
                    // Spawn sample mesh.
                    const auto pMeshNode = gc_new<MeshNode>();
                    auto meshData = PrimitiveMeshGenerator::createCube(1.0F);
                    meshData.getIndices()->at(0) = {
                        0,  1,  2,  3,  2,  1,  // +X face.
                        12, 13, 14, 15, 14, 13, // -Y face.
                        16, 17, 18, 19, 18, 17, // +Z face.
                        20, 21, 22, 23, 22, 21  // -Z face.
                    };
                    meshData.getIndices()->push_back({4, 5, 6, 7, 6, 5});
                    pMeshNode->setMeshData(meshData);
                    REQUIRE(pMeshNode->getAvailableMaterialSlotCount() == 2);

                    getWorldRootNode()->addChildNode(pMeshNode);
                    pMeshNode->setWorldLocation(glm::vec3(1.0F, 3.0F, 0.0F));

                    // There should only be 1 pipeline.
                    REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 1);

                    // Enable transparency on the second material slot.
                    pMeshNode->getMaterial(1)->setEnableTransparency(true);

                    // There should now be 2 pipelines.
                    REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 2);
                }

                {
                    // Spawn another mesh.
                    const auto pMeshNode = gc_new<MeshNode>();
                    pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                    REQUIRE(pMeshNode->getAvailableMaterialSlotCount() == 1);

                    getWorldRootNode()->addChildNode(pMeshNode);
                    pMeshNode->setWorldLocation(glm::vec3(1.0F, 3.0F, 0.0F));

                    // There should still be 2 pipelines.
                    REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 2);
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

TEST_CASE("serialize and deserialize mesh with 2 material slots") {
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
                    "TESTING_MeshNodeSerializationMaterialSlots_TESTING.toml";

                {
                    // Spawn sample mesh.
                    const auto pMeshNode = gc_new<MeshNode>();
                    auto meshData = PrimitiveMeshGenerator::createCube(1.0F);
                    meshData.getIndices()->at(0) = {
                        0,  1,  2,  3,  2,  1,  // +X face.
                        12, 13, 14, 15, 14, 13, // -Y face.
                        16, 17, 18, 19, 18, 17, // +Z face.
                        20, 21, 22, 23, 22, 21  // -Z face.
                    };
                    meshData.getIndices()->push_back({4, 5, 6, 7, 6, 5});
                    pMeshNode->setMeshData(meshData);
                    REQUIRE(pMeshNode->getAvailableMaterialSlotCount() == 2);

                    getWorldRootNode()->addChildNode(pMeshNode);
                    pMeshNode->setWorldLocation(glm::vec3(1.0F, 3.0F, 0.0F));

                    // Enable transparency on the second material slot.
                    pMeshNode->getMaterial(1)->setEnableTransparency(true);

                    // Serialize.
                    auto optionalError = pMeshNode->serialize(pathToFileInTemp, false);
                    if (optionalError.has_value()) {
                        optionalError->addCurrentLocationToErrorStack();
                        INFO(optionalError->getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                // Deserialize.
                auto result = Serializable::deserialize<gc, MeshNode>(pathToFileInTemp);
                if (std::holds_alternative<Error>(result)) {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                auto pMeshNode = std::get<gc<MeshNode>>(std::move(result));

                // Make sure there are 2 slots.
                REQUIRE(pMeshNode->getMeshData().second->getIndices()->size() == 2);
                REQUIRE(pMeshNode->getAvailableMaterialSlotCount() == 2);

                // Check transparency.
                REQUIRE(!pMeshNode->getMaterial(0)->isTransparencyEnabled());
                REQUIRE(pMeshNode->getMaterial(1)->isTransparencyEnabled());

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
