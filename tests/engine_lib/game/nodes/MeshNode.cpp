// Custom.
#include "game/nodes/MeshNode.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "materials/Material.h"

// External.
#include "catch2/catch_test_macros.hpp"

TEST_CASE("serialize and deserialize MeshNode") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : GameInstance(pGameWindow, pInputManager) {}
        virtual void onGameStarted() override {
            createWorld();

            const std::filesystem::path pathToFileInTemp =
                std::filesystem::temp_directory_path() / "TESTING_MeshNodeSerialization_TESTING.toml";

            {
                // Create material.
                auto result = Material::create(true, "My Material");
                if (std::holds_alternative<Error>(result)) {
                    Error error = std::get<Error>(std::move(result));
                    error.addEntry();
                    INFO(error.getError());
                    REQUIRE(false);
                }

                // Create node and set material.
                const auto pMeshNode = gc_new<MeshNode>("My cool node");
                pMeshNode->setMaterial(std::get<std::shared_ptr<Material>>(std::move(result)));

                // Serialize node.
                auto optionalError = pMeshNode->serialize(pathToFileInTemp, false);
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addEntry();
                    INFO(error.getError());
                    REQUIRE(false);
                }
            }

            gc_collector()->collect();
            REQUIRE(Material::getTotalMaterialCount() == 0);

            {
                // Deserialize.
                auto result = Serializable::deserialize<gc, MeshNode>(pathToFileInTemp);
                if (std::holds_alternative<Error>(result)) {
                    Error error = std::get<Error>(std::move(result));
                    error.addEntry();
                    INFO(error.getError());
                    REQUIRE(false);
                }

                const auto pMeshNode = std::get<gc<MeshNode>>(std::move(result));

                // Check.
                REQUIRE(pMeshNode->getName() == "My cool node");
                REQUIRE(pMeshNode->getMaterial()->isUsingTransparency());
                REQUIRE(pMeshNode->getMaterial()->getName() == "My Material");
            }

            gc_collector()->collect();
            REQUIRE(Material::getTotalMaterialCount() == 0);

            // Cleanup.
            if (std::filesystem::exists(pathToFileInTemp)) {
                std::filesystem::remove(pathToFileInTemp);
            }

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
