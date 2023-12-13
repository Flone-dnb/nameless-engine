#if defined(WIN32)
#include <Windows.h>
#include <crtdbg.h>
#undef MessageBox
#undef IGNORE
#endif

// Standard.
#include <filesystem>

// Custom.
#include "misc/Globals.h"
#include "game/Window.h"
#include "misc/ProjectPaths.h"
#include "game/GameInstance.h"
#include "render/vulkan/VulkanRenderer.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif

// External.
#include "catch2/catch_session.hpp"

int main() {
// Enable run-time memory check for debug builds.
#if defined(DEBUG) && defined(WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#elif defined(WIN32) && !defined(DEBUG)
    OutputDebugStringA("Using release build configuration, memory checks are disabled.");
#endif

    // Clear any config files.
    const std::filesystem::path baseConfigPath =
        ne::ProjectPaths::getPathToBaseConfigDirectory() / ne::Globals::getApplicationName();
    const auto baseTempPath =
        ne::ProjectPaths::getPathToResDirectory(ne::ResourceDirectory::ROOT) / "test" / "temp";

    if (std::filesystem::exists(baseConfigPath)) {
        std::filesystem::remove_all(baseConfigPath);
    }
    if (std::filesystem::exists(baseTempPath)) {
        std::filesystem::remove_all(baseTempPath);
    }

#if !defined(WIN32)
    // Just run tests with available renderer.
    return Catch::Session().run();
#else
    static bool bIsDirectXRendererSupported = false;
    static bool bIsVulkanRendererSupported = false;

    // Prepare a game instance that will test if DirectX renderer is supported.
    class DirectXGameInstance : public ne::GameInstance {
    public:
        virtual ~DirectXGameInstance() override = default;
        DirectXGameInstance(ne::Window* pGameWindow, ne::GameManager* pGame, ne::InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            if (dynamic_cast<ne::DirectXRenderer*>(getWindow()->getRenderer()) != nullptr) {
                bIsDirectXRendererSupported = true;
            } else {
                bIsDirectXRendererSupported = false;
            }

            getWindow()->close();
        }
    };

    // Prepare a game instance that will test if Vulkan renderer is supported.
    class VulkanGameInstance : public ne::GameInstance {
    public:
        virtual ~VulkanGameInstance() override = default;
        VulkanGameInstance(ne::Window* pGameWindow, ne::GameManager* pGame, ne::InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}
        virtual void onGameStarted() override {
            if (dynamic_cast<ne::VulkanRenderer*>(getWindow()->getRenderer()) != nullptr) {
                bIsVulkanRendererSupported = true;
            } else {
                bIsVulkanRendererSupported = false;
            }

            getWindow()->close();
        }
    };

    {
        // Create a new window.
        auto result = ne::Window::getBuilder().withVisibility(false).build();
        if (std::holds_alternative<ne::Error>(result)) {
            auto error = std::get<ne::Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            ne::Logger::get().error(error.getFullErrorMessage());
            return 1;
        }
        const std::unique_ptr<ne::Window> pWindow = std::get<std::unique_ptr<ne::Window>>(std::move(result));

        // Since we are on Windows, test that DirectX renderer is supported.
        ne::Logger::get().info("attempting to run tests using DirectX renderer...");
        pWindow->setPreferredRenderer(ne::RendererType::DIRECTX); // force DirectX renderer
        pWindow->processEvents<DirectXGameInstance>();
    }

    Catch::Session session;

    // Check DirectX support.
    if (bIsDirectXRendererSupported) {
        // Run tests on DirectX renderer.
        const auto iReturnCode = session.run();
        if (iReturnCode != 0) {
            ne::Logger::get().error(std::format(
                "some tests failed using supported DirectX renderer, error code: {}", iReturnCode));
            return iReturnCode;
        }
        ne::Logger::get().info("all tests passed using DirectX renderer");

        // Clear some config files (don't remove log file as the logger is writing to it).
        const auto baseTempPath =
            ne::ProjectPaths::getPathToResDirectory(ne::ResourceDirectory::ROOT) / "test" / "temp";
        if (std::filesystem::exists(ne::ProjectPaths::getPathToPlayerSettingsDirectory())) {
            std::filesystem::remove_all(ne::ProjectPaths::getPathToPlayerSettingsDirectory());
        }
        if (std::filesystem::exists(ne::ProjectPaths::getPathToPlayerProgressDirectory())) {
            std::filesystem::remove_all(ne::ProjectPaths::getPathToPlayerProgressDirectory());
        }
        if (std::filesystem::exists(baseTempPath)) {
            std::filesystem::remove_all(baseTempPath);
        }
    } else {
        ne::Logger::get().info("DirectX renderer is not supported");
    }

    {
        // Create a new window.
        auto result = ne::Window::getBuilder().withVisibility(false).build();
        if (std::holds_alternative<ne::Error>(result)) {
            auto error = std::get<ne::Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            ne::Logger::get().error(error.getFullErrorMessage());
            return 1;
        }
        const std::unique_ptr<ne::Window> pWindow = std::get<std::unique_ptr<ne::Window>>(std::move(result));

        // Test that Vulkan renderer is supported.
        ne::Logger::get().info("attempting to run tests using Vulkan renderer...");
        pWindow->setPreferredRenderer(ne::RendererType::VULKAN); // force Vulkan renderer
        pWindow->processEvents<VulkanGameInstance>();
    }

    // Check Vulkan support.
    if (bIsVulkanRendererSupported) {
        // Run tests on Vulkan renderer.
        const auto iReturnCode = session.run();
        if (iReturnCode != 0) {
            ne::Logger::get().error(std::format(
                "some tests failed using supported Vulkan renderer, error code: {}", iReturnCode));
            return iReturnCode;
        }
        ne::Logger::get().info("all tests passed using Vulkan renderer");
    } else {
        ne::Logger::get().info("Vulkan renderer is not supported");
    }

    ne::Logger::get().info("finished testing");

    // Final logs.
    if (!bIsDirectXRendererSupported && !bIsVulkanRendererSupported) {
        ne::Logger::get().error("no renderer is supported");
        return 1;
    }

    // Check warnings/errors logged.
    if (ne::Logger::getTotalWarningsProduced() > 0 || ne::Logger::getTotalErrorsProduced() > 0) {
        ne::Logger::get().info("all tests passed but some warnings/errors were logged");
        return 1;
    }

    if (bIsDirectXRendererSupported && bIsVulkanRendererSupported) {
        ne::Logger::get().info("both DirectX and Vulkan renderers were tested and all tests passed");
        return 0;
    }

    if (bIsDirectXRendererSupported) {
        ne::Logger::get().info(
            "only DirectX renderer was tested (Vulkan is not supported) and all tests passed");
        return 0;
    }

    if (bIsVulkanRendererSupported) {
        ne::Logger::get().info(
            "only Vulkan renderer was tested (DirectX is not supported) and all tests passed");
        return 0;
    }

    return 0;
#endif
}
