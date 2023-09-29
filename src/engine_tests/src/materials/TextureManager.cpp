// Custom.
#include "game/GameInstance.h"
#include "game/Window.h"
#include "misc/ProjectPaths.h"
#include "materials/TextureManager.h"
#if defined(WIN32)
#include "render/directx/resources/DirectXResource.h"
#endif

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

TEST_CASE("import texture") {
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
                // Prepare some paths.
                constexpr auto pImportedTextureDirectoryName = "imported";
                const auto pathToImportexTextureDir =
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "temp" /
                    pImportedTextureDirectoryName;

                // Delete previously imported texture (if exists).
                if (std::filesystem::exists(pathToImportexTextureDir)) {
                    std::filesystem::remove_all(pathToImportexTextureDir);
                }

                // Import sample texture.
                auto optionalError = TextureManager::importTexture(
                    ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) / "test" / "texture.png",
                    TextureType::DIFFUSE,
                    "test/temp",
                    pImportedTextureDirectoryName,
                    textureImportProcess);
                if (optionalError.has_value()) {
                    optionalError->addCurrentLocationToErrorStack();
                    INFO(optionalError->getFullErrorMessage());
                    REQUIRE(false);
                }

                // Check that resulting texture has mipmaps.
                auto result =
                    getWindow()->getRenderer()->getResourceManager()->getTextureManager()->getTexture(
                        std::string("test/temp/") + pImportedTextureDirectoryName);
                if (std::holds_alternative<Error>(result)) {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }
                const auto pTextureHandle = std::get<std::unique_ptr<TextureHandle>>(std::move(result));

#if defined(WIN32)
                if (auto pDirectXResource = dynamic_cast<DirectXResource*>(pTextureHandle->getResource())) {

                    const auto resourceDesc = pDirectXResource->getInternalResource()->GetDesc();
                    REQUIRE(resourceDesc.MipLevels == 7);

                    getWindow()->close();
                    return;
                }
#endif

                SKIP();
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
}
