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

TEST_CASE("unable to compile shader with a name that contains forbidden character") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            // Prepare shaders to compile.
            std::vector<ShaderDescription> vShadersToCompile = {ShaderDescription(
                "?s",
                ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                    "test/shaders/hlsl/CustomMeshNode.hlsl",
                ShaderType::VERTEX_SHADER,
                "vsCustomMeshNode",
                {})};

            // Attempt to compile.
            const auto optionalError = getWindow()->getRenderer()->getShaderManager()->compileShaders(
                std::move(vShadersToCompile),
                [](size_t iCompiled, size_t iTotal) { REQUIRE(false); },
                [](ShaderDescription description, std::variant<std::string, Error> error) { REQUIRE(false); },
                []() { REQUIRE(false); });

            // Should have an error.
            REQUIRE(optionalError.has_value());

            getWindow()->close();
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
