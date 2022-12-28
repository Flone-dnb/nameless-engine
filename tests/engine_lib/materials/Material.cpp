// Custom.
#include "materials/Material.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "game/nodes/MeshNode.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("create engine default materials") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld();

            {
                // Create material.
                auto resultOpaque = Material::create(false);
                if (std::holds_alternative<Error>(resultOpaque)) {
                    Error error = std::get<Error>(std::move(resultOpaque));
                    error.addEntry();
                    INFO(error.getError());
                    REQUIRE(false);
                }
                auto resultTransparent = Material::create(true);
                if (std::holds_alternative<Error>(resultTransparent)) {
                    Error error = std::get<Error>(std::move(resultTransparent));
                    error.addEntry();
                    INFO(error.getError());
                    REQUIRE(false);
                }

                // Create nodes.
                auto pMeshNodeOpaque = gc_new<MeshNode>("Opaque material node");
                pMeshNodeOpaque->setMaterial(std::get<std::shared_ptr<Material>>(std::move(resultOpaque)));

                auto pMeshNodeTransparent = gc_new<MeshNode>("Transparent material node");
                pMeshNodeTransparent->setMaterial(
                    std::get<std::shared_ptr<Material>>(std::move(resultTransparent)));

                // There should be no PSOs created since no material is spawned.
                REQUIRE(getWindow()->getRenderer()->getPsoManager()->getCreatedGraphicsPsoCount() == 0);

                // Spawn.
                getWorldRootNode()->addChildNode(pMeshNodeOpaque);
                getWorldRootNode()->addChildNode(pMeshNodeTransparent);

                // Check everything.
                REQUIRE(Material::getTotalMaterialCount() == 2);
                REQUIRE(getWindow()->getRenderer()->getPsoManager()->getCreatedGraphicsPsoCount() == 2);

                // Despawn one node.
                pMeshNodeOpaque->detachFromParentAndDespawn();
                REQUIRE(Material::getTotalMaterialCount() == 2);
                REQUIRE(getWindow()->getRenderer()->getPsoManager()->getCreatedGraphicsPsoCount() == 1);
            }

            // Despawn all nodes.
            createWorld();

            // Check that everything is cleaned up.
            REQUIRE(Material::getTotalMaterialCount() == 0);
            REQUIRE(getWindow()->getRenderer()->getPsoManager()->getCreatedGraphicsPsoCount() == 0);

            getWindow()->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();

    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
    REQUIRE(Material::getTotalMaterialCount() == 0);
}

// TEST_CASE("serialize and deserialize Material") {
//     using namespace ne;

//    class TestGameInstance : public GameInstance {
//    public:
//        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
//            : GameInstance(pGameWindow, pInputManager) {}
//        virtual void onGameStarted() override {
//            createWorld();

//            const std::filesystem::path pathToFileInTemp =
//                std::filesystem::temp_directory_path() / "TESTING_MaterialSerialization_TESTING.toml";

//            {
//                // Create material.
//                auto result = Material::create(true, "My Material");
//                if (std::holds_alternative<Error>(result)) {
//                    Error error = std::get<Error>(std::move(result));
//                    error.addEntry();
//                    INFO(error.getError());
//                    REQUIRE(false);
//                }

//                // Serialize.
//                const auto pMaterial = std::get<std::shared_ptr<Material>>(std::move(result));
//                auto optionalError = pMaterial->serialize(pathToFileInTemp, false);
//                if (optionalError.has_value()) {
//                    auto error = optionalError.value();
//                    error.addEntry();
//                    INFO(error.getError());
//                    REQUIRE(false);
//                }
//            }

//            REQUIRE(Material::getTotalMaterialCount() == 0);

//            {
//                // Deserialize.
//                auto result = Serializable::deserialize<Material, std::shared_ptr>(pathToFileInTemp);
//                if (std::holds_alternative<Error>(result)) {
//                    Error error = std::get<Error>(std::move(result));
//                    error.addEntry();
//                    INFO(error.getError());
//                    REQUIRE(false);
//                }

//                const auto pMaterial = std::get<std::shared_ptr<Material>>(std::move(result));

//                // Check.
//                REQUIRE(pMaterial->getName() == "My Material");
//                REQUIRE(pMaterial->isUsingTransparency());
//            }

//            // Cleanup.
//            if (std::filesystem::exists(pathToFileInTemp)) {
//                std::filesystem::remove(pathToFileInTemp);
//            }

//            getWindow()->close();
//        }
//        virtual ~TestGameInstance() override {}
//    };

//    auto result = Window::getBuilder().withVisibility(false).build();
//    if (std::holds_alternative<Error>(result)) {
//        Error error = std::get<Error>(std::move(result));
//        error.addEntry();
//        INFO(error.getError());
//        REQUIRE(false);
//    }

//    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
//    pMainWindow->processEvents<TestGameInstance>();

//    REQUIRE(gc_collector()->getAliveObjectsCount() == 0);
//    REQUIRE(Material::getTotalMaterialCount() == 0);
//}
