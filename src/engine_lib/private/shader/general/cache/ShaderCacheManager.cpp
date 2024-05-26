#include "ShaderCacheManager.h"

// Custom.
#include "shader/general/ShaderFilesystemPaths.hpp"
#include "render/vulkan/VulkanRenderer.h"
#include "shader/glsl/cache/GlslShaderCacheManager.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "shader/hlsl/cache/HlslShaderCacheManager.h"
#endif

namespace ne {
    std::unique_ptr<ShaderCacheManager> ShaderCacheManager::create(Renderer* pRenderer) {
        // GLSL.
        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            return std::unique_ptr<GlslShaderCacheManager>(new GlslShaderCacheManager(pRenderer));
        }

#if defined(WIN32)
        // HLSL.
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            return std::unique_ptr<HlslShaderCacheManager>(new HlslShaderCacheManager(pRenderer));
        }
#endif

        Error error("unexpected renderer");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    ShaderCacheManager::ShaderCacheManager(Renderer* pRenderer) : pRenderer(pRenderer) {}

    std::optional<Error> ShaderCacheManager::refreshShaderCache() {
        // Get build mode.
#if defined(DEBUG)
        constexpr bool bIsReleaseBuild = false;
#else
        constexpr bool bIsReleaseBuild = true;
#endif

        ConfigManager cacheConfig;

        // Prepare paths.
        const auto pathToShaderCacheDir = ShaderFilesystemPaths::getPathToShaderCacheDirectory();
        const auto pathToShaderCacheFile = pathToShaderCacheDir / sGlobalShaderCacheInfoFileName;

        bool bRecreateShaderCache = false;

        // Check if cache exists.
        if (!std::filesystem::exists(pathToShaderCacheFile)) {
            Logger::get().info(std::format(
                "global shader cache configuration was not found, creating a new {} configuration",
                bIsReleaseBuild ? "release" : "debug"));

            bRecreateShaderCache = true;
        } else {
            // Load shader cache file.
            auto optionalError = cacheConfig.loadFile(pathToShaderCacheFile);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError.value();
            }

            // Check if it's outdated.
            const auto optionalReason = isGlobalShaderCacheOutdated(cacheConfig);
            if (optionalReason.has_value()) {
                Logger::get().info(std::format(
                    "global shader cache configuration is outdated, reason: {}, a new configuration will be "
                    "created",
                    optionalReason.value()));

                bRecreateShaderCache = true;
            }
        }

        if (!bRecreateShaderCache) {
            return {};
        }

        // Create new cache.
        auto optionalError = createNewShaderCache();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> ShaderCacheManager::createNewShaderCache() {
        // Get build mode.
#if defined(DEBUG)
        constexpr bool bIsReleaseBuild = false;
#else
        constexpr bool bIsReleaseBuild = true;
#endif

        // Prepare paths.
        const auto pathToShaderCacheDir = ShaderFilesystemPaths::getPathToShaderCacheDirectory();
        const auto pathToShaderCacheFile = pathToShaderCacheDir / sGlobalShaderCacheInfoFileName;

        // Re-create cache directory.
        if (std::filesystem::exists(pathToShaderCacheDir)) {
            std::filesystem::remove_all(pathToShaderCacheDir);
            std::filesystem::create_directory(pathToShaderCacheDir);
        }

        ConfigManager cacheConfig;

        // Write build mode.
        cacheConfig.setValue<bool>("", GlobalShaderCacheParameterNames::sIsReleaseBuild, bIsReleaseBuild);

        // Write renderer.
        cacheConfig.setValue<unsigned int>(
            "", GlobalShaderCacheParameterNames::sRenderer, static_cast<unsigned int>(pRenderer->getType()));

        // Write language-specific parameters.
        auto optionalError = writeLanguageSpecificParameters(cacheConfig);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Save the new global shader cache file.
        optionalError = cacheConfig.saveFile(pathToShaderCacheFile, false);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return {};
    }

    std::optional<std::string>
    ShaderCacheManager::isGlobalShaderCacheOutdated(const ConfigManager& cacheConfig) {
        // Get build mode.
#if defined(DEBUG)
        constexpr bool bIsReleaseBuild = false;
#else
        constexpr bool bIsReleaseBuild = true;
#endif

        {
            // Read build mode.
            const auto bOldIsReleaseBuild = cacheConfig.getValue<bool>(
                "", GlobalShaderCacheParameterNames::sIsReleaseBuild, !bIsReleaseBuild);

            // Compare build mode.
            if (bOldIsReleaseBuild != bIsReleaseBuild) {
                return "build mode changed";
            }
        }

        {
            // Read renderer.
            const auto iOldRenderer = cacheConfig.getValue<unsigned int>(
                "", GlobalShaderCacheParameterNames::sRenderer, std::numeric_limits<unsigned int>::max());

            // Compare renderer.
            if (static_cast<unsigned int>(pRenderer->getType()) != iOldRenderer) {
                return "renderer changed";
            }
        }

        // Check language specific parameters.
        auto optionalReason = isLanguageSpecificGlobalCacheOutdated(cacheConfig);
        if (optionalReason.has_value()) {
            return optionalReason;
        }

        return {};
    }

}
