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
                auto resultTransparent = Material::create(false);
                if (std::holds_alternative<Error>(resultTransparent)) {
                    Error error = std::get<Error>(std::move(resultTransparent));
                    error.addEntry();
                    INFO(error.getError());
                    REQUIRE(false);
                }

                // Create nodes.
                auto pMeshNodeOpaque = gc_new<MeshNode>("Opaque material node");
                pMeshNodeOpaque->setMaterial(std::get<gc<Material>>(std::move(resultOpaque)));

                auto pMeshNodeTransparent = gc_new<MeshNode>("Transparent material node");
                pMeshNodeTransparent->setMaterial(std::get<gc<Material>>(std::move(resultTransparent)));

                // There should be no PSOs created since no material is spawned.
                REQUIRE(getWindow()->getRenderer()->getPsoManager()->getCreatedGraphicsPsoCount() == 0);

                // Spawn.
                getWorldRootNode()->addChildNode(pMeshNodeOpaque);
                getWorldRootNode()->addChildNode(pMeshNodeTransparent);

                gc_collect();

                // Check everything.
                REQUIRE(Material::getTotalMaterialCount() == 2);
                REQUIRE(getWindow()->getRenderer()->getPsoManager()->getCreatedGraphicsPsoCount() == 1);

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
