// Custom.
#include "game/node/MeshNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/camera/CameraManager.h"
#include "game/node/CameraNode.h"
#include "material/Material.h"
#include "shader/general/EngineShaderNames.hpp"
#include "io/ReflectionTest.h"
#include "misc/PrimitiveMeshGenerator.h"
#include "shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBindingManager.h"
#include "TestHelpers.hpp"
#include "render/general/pipeline/PipelineManager.h"

// External.
#include "catch2/catch_test_macros.hpp"

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
                        EngineShaderNames::MeshNode::getVertexShaderName(),
                        EngineShaderNames::MeshNode::getFragmentShaderName(),
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
                    const auto pMeshNode = sgc::makeGc<MeshNode>("My cool node");
                    pMeshNode->setMaterial(std::get<std::unique_ptr<Material>>(std::move(result)));
                    pMeshNode->setMeshData(std::move(meshData));
                    getWorldRootNode()->addChildNode(
                        pMeshNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

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
                    const sgc::GcPtr<MeshNode> pMeshNode =
                        dynamic_cast<MeshNode*>(pRootNode->getChildNodes()->second[0].get());

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

                sgc::GarbageCollector::get().collectGarbage();

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
                        EngineShaderNames::MeshNode::getVertexShaderName(),
                        EngineShaderNames::MeshNode::getFragmentShaderName(),
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
                    const auto pMeshNode = sgc::makeGc<MeshNode>("My cool node");
                    pMeshNode->setMaterial(std::get<std::unique_ptr<Material>>(std::move(result)));
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
                    auto result = Serializable::deserialize<sgc::GcPtr<MeshNode>>(pathToNodeFile);
                    if (std::holds_alternative<Error>(result)) {
                        auto error = std::get<Error>(result);
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                    auto pMeshNode = std::get<sgc::GcPtr<MeshNode>>(std::move(result));

                    getWorldRootNode()->addChildNode(
                        pMeshNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

                    // Serialize tree.
                    auto optionalError = getWorldRootNode()->serializeNodeTree(pathToFileInTemp, false);
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

                MeshVertex vertex3;
                vertex3.position = glm::vec3(-1.0f, -2.0f, -3.0f);
                vertex3.uv = glm::vec2(-1.0f, -2.0f);

                {
                    // Modify mesh data.
                    sgc::GcPtr<MeshNode> pMeshNode =
                        dynamic_cast<MeshNode*>(getWorldRootNode()->getChildNodes()->second[0].get());

                    MeshData meshData;
                    meshData.getVertices()->push_back(vertex1);
                    meshData.getVertices()->push_back(vertex2);
                    meshData.getVertices()->push_back(vertex3);
                    meshData.getIndices()->push_back({0, 1, 2});

                    pMeshNode->setMeshData(meshData);

                    // Serialize tree again.
                    auto optionalError = getWorldRootNode()->serializeNodeTree(pathToFileInTemp, false);
                    if (optionalError.has_value()) {
                        auto error = optionalError.value();
                        error.addCurrentLocationToErrorStack();
                        INFO(error.getFullErrorMessage());
                        REQUIRE(false);
                    }
                }

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
                    const sgc::GcPtr<MeshNode> pMeshNode =
                        dynamic_cast<MeshNode*>(pRootNode->getChildNodes()->second[0].get());

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

                sgc::GarbageCollector::get().collectGarbage();

                // Cleanup.
                if (std::filesystem::exists(pathToFileInTemp)) {
                    ConfigManager::removeFile(pathToFileInTemp);
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

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
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
                    "MeshNodeDeserializationCompatibility.toml";

                // Create mesh data.
                MeshVertex vertex1, vertex2;
                vertex1.position = glm::vec3(5123.91827f, -12225.24142f, -5.0f);
                vertex1.normal = glm::vec3(10.0f, -1111.22212f, 0.0f);
                vertex1.uv = glm::vec3(10.0f, -8885.14122f, 0.0f);
                vertex2.position = glm::vec3(-1.0f, -2.0f, -3.0f);
                vertex2.normal = glm::vec3(-1.0f, 0.0f, 0.0f);
                vertex2.uv = glm::vec2(-1.0f, -2.0f);

#if defined(DEBUG) && defined(WIN32)
                static_assert(sizeof(MeshData) == 160, "add new fields here"); // NOLINT
#elif defined(DEBUG)
                static_assert(sizeof(MeshData) == 128, "add new fields here"); // NOLINT
#endif

                // Deserialize.
                auto result = Serializable::deserialize<sgc::GcPtr<MeshNode>>(pathToFileInTemp);
                if (std::holds_alternative<Error>(result)) {
                    Error error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                const auto pMeshNode = std::get<sgc::GcPtr<MeshNode>>(std::move(result));

                // Check.
                const auto mtxMeshData = pMeshNode->getMeshData();
                std::scoped_lock guard(*mtxMeshData.first);

                // Check vertices.
                REQUIRE(mtxMeshData.second->getVertices()->size() == 2);
                REQUIRE(mtxMeshData.second->getVertices()->at(0) == vertex1);
                REQUIRE(mtxMeshData.second->getVertices()->at(1) == vertex2);

                // Check indices.
                REQUIRE(mtxMeshData.second->getIndices()->size() == 2);
                REQUIRE(mtxMeshData.second->getIndices()->at(0).size() == 3);
                REQUIRE(mtxMeshData.second->getIndices()->at(0)[0] == 0);
                REQUIRE(mtxMeshData.second->getIndices()->at(0)[1] == 1);
                REQUIRE(mtxMeshData.second->getIndices()->at(0)[2] == 2);
                REQUIRE(mtxMeshData.second->getIndices()->at(1).size() == 3);
                REQUIRE(mtxMeshData.second->getIndices()->at(1)[0] == 3);
                REQUIRE(mtxMeshData.second->getIndices()->at(1)[1] == 4);
                REQUIRE(mtxMeshData.second->getIndices()->at(1)[2] == 5);

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
                    EngineShaderNames::MeshNode::getVertexShaderName(),
                    EngineShaderNames::MeshNode::getFragmentShaderName(),
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
                const auto pMeshNode = sgc::makeGc<MeshNode>("My cool node");
                pMeshNode->setMaterial(std::get<std::unique_ptr<Material>>(std::move(result)));
                pMeshNode->setMeshData(std::move(meshData));

                // Get shader resource manager.
                const auto pShaderCpuWriteResourceManager =
                    getWindow()->getRenderer()->getShaderCpuWriteResourceManager();
                auto pMtxResources = pShaderCpuWriteResourceManager->getResources();

                // Make sure no shader read/write resources created.
                std::scoped_lock shaderCpuWriteResourcesGuard(pMtxResources->first);
                REQUIRE(pMtxResources->second.all.empty());
                for (const auto& set : pMtxResources->second.vToBeUpdated) {
                    REQUIRE(set.empty());
                }

                // Save VRAM to check later.
                const auto iVramMbNotSpawned = getWindow()->getRenderer()->getUsedVideoMemoryInMb();

                // Spawn mesh node.
                getWorldRootNode()->addChildNode(
                    pMeshNode, Node::AttachmentRule::KEEP_RELATIVE, Node::AttachmentRule::KEEP_RELATIVE);

                // Make sure there are 2 resources (meshData and materialData).
                REQUIRE(pMtxResources->second.all.size() == 2);
                for (const auto& set : pMtxResources->second.vToBeUpdated) {
                    REQUIRE(set.size() == 2);
                }

                // Check VRAM.
                const auto iVramMbSpawned = getWindow()->getRenderer()->getUsedVideoMemoryInMb();
                REQUIRE(iVramMbSpawned > iVramMbNotSpawned);

                // Self check for spawned mesh size.
                constexpr size_t iMaxMeshSizeForTestMb = 512;
                if (iVramMbSpawned - iVramMbNotSpawned > iMaxMeshSizeForTestMb) {
                    Error error(std::format(
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
                for (const auto& set : pMtxResources->second.vToBeUpdated) {
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

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
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

                auto pCamera =
                    TestHelpers::createAndSpawnActiveCamera(getWorldRootNode(), getCameraManager());
                pCamera->setRelativeLocation(glm::vec3(-1.0F, 0.0F, 0.0F));

                // Spawn sample mesh.
                pMeshNode = sgc::makeGc<MeshNode>();

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
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float delta) override {
            iFrameCount += 1;

            if (iFrameCount == 2) {
                // Make sure something was rendered (in case we forgot the camera).
                REQUIRE(getWindow()->getRenderer()->getRenderStatistics()->getLastFrameDrawCallCount() > 0);

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
        sgc::GcPtr<MeshNode> pMeshNode;
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
                    const auto pMeshNode = sgc::makeGc<MeshNode>();
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

                    // 1 opaque + depth only + 2 shadow mapping.
                    REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 4);

                    // Enable transparency on the second material slot.
                    pMeshNode->getMaterial(1)->setEnableTransparency(true);

                    // 1 opaque + depth only + 2 shadow mapping + transparent.
                    REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 5);
                }

                {
                    // Spawn another mesh.
                    const auto pMeshNode = sgc::makeGc<MeshNode>();
                    pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                    REQUIRE(pMeshNode->getAvailableMaterialSlotCount() == 1);

                    getWorldRootNode()->addChildNode(pMeshNode);
                    pMeshNode->setWorldLocation(glm::vec3(1.0F, 3.0F, 0.0F));

                    // There should still be 5 pipelines.
                    REQUIRE(pPipelineManager->getCurrentGraphicsPipelineCount() == 5);
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
                    const auto pMeshNode = sgc::makeGc<MeshNode>();
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
                auto result = Serializable::deserialize<sgc::GcPtr<MeshNode>>(pathToFileInTemp);
                if (std::holds_alternative<Error>(result)) {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                auto pMeshNode = std::get<sgc::GcPtr<MeshNode>>(std::move(result));

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

    REQUIRE(sgc::GarbageCollector::get().getAliveAllocationCount() == 0);
    REQUIRE(Material::getCurrentAliveMaterialCount() == 0);
}

TEST_CASE("check draw call count with invisibility") {
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

                // Create and setup camera.
                // Create camera.
                auto pCamera =
                    TestHelpers::createAndSpawnActiveCamera(getWorldRootNode(), getCameraManager());
                pCamera->setRelativeLocation(glm::vec3(-1.0F, 0.0F, 0.0F));

                // Make it active.
                getCameraManager()->setActiveCamera(pCamera);

                // Spawn sample mesh.
                const auto pMeshNode = sgc::makeGc<MeshNode>();
                pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                getWorldRootNode()->addChildNode(pMeshNode);
                pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

                iFrameCount = 0;
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float delta) override {
            iFrameCount += 1;

            if (iFrameCount == 3) {
                REQUIRE(getWindow()->getRenderer()->getRenderStatistics()->getLastFrameDrawCallCount() >= 1);

                // Spawn another sample mesh.
                pSomeMeshNode = sgc::makeGc<MeshNode>();
                pSomeMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                getWorldRootNode()->addChildNode(pSomeMeshNode);
                pSomeMeshNode->setWorldLocation(glm::vec3(1.0F, 3.0F, 0.0F));
            }

            if (iFrameCount == 5) {
                REQUIRE(getWindow()->getRenderer()->getRenderStatistics()->getLastFrameDrawCallCount() >= 2);

                // Make one mesh invisible.
                pSomeMeshNode->setIsVisible(false);
            }

            if (iFrameCount == 7) {
                REQUIRE(getWindow()->getRenderer()->getRenderStatistics()->getLastFrameDrawCallCount() >= 1);
                getWindow()->close();
            }
        }

    private:
        size_t iFrameCount = 0;
        sgc::GcPtr<MeshNode> pSomeMeshNode;
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

TEST_CASE("check draw call count with frustum culling") {
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

                // Create and setup camera.
                // Create camera.
                pCamera = TestHelpers::createAndSpawnActiveCamera(getWorldRootNode(), getCameraManager());
                pCamera->setRelativeLocation(glm::vec3(-1.0F, 0.0F, 0.0F));

                // Spawn sample mesh.
                const auto pMeshNode = sgc::makeGc<MeshNode>();
                pMeshNode->setMeshData(PrimitiveMeshGenerator::createCube(1.0F));
                getWorldRootNode()->addChildNode(pMeshNode);
                pMeshNode->setWorldLocation(glm::vec3(1.0F, 0.0F, 0.0F));

                iFrameCount = 0;
            });
        }
        virtual ~TestGameInstance() override {}

        virtual void onBeforeNewFrame(float delta) override {
            iFrameCount += 1;

            const auto pRenderer = getWindow()->getRenderer();

            if (iFrameCount == 2) {
                REQUIRE(pRenderer->getRenderStatistics()->getLastFrameDrawCallCount() > 0);
                REQUIRE(pRenderer->getRenderStatistics()->getLastFrameCulledMeshCount() == 0);

                // Rotate the camera 180 degrees.
                pCamera->setRelativeRotation(glm::vec3(0.0F, 0.0F, 180.0F));
            }

            if (iFrameCount == 3) {
                REQUIRE(pRenderer->getRenderStatistics()->getLastFrameDrawCallCount() == 0);
                REQUIRE(pRenderer->getRenderStatistics()->getLastFrameCulledMeshCount() == 1);

                getWindow()->close();
            }
        }

    private:
        size_t iFrameCount = 0;
        sgc::GcPtr<CameraNode> pCamera;
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
