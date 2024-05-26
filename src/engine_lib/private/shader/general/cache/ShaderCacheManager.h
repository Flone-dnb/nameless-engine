#pragma once

// Standard.
#include <memory>
#include <string>
#include <optional>

// Custom.
#include "misc/Error.h"
#include "io/ConfigManager.h"

namespace ne {
    class Renderer;

    /** Responsible for validating shader cache, reading and updating the cache. */
    class ShaderCacheManager {
        // Only shader manager is expected to create this manager.
        friend class ShaderManager;

    public:
        virtual ~ShaderCacheManager() = default;

        ShaderCacheManager() = delete;
        ShaderCacheManager(const ShaderCacheManager&) = delete;
        ShaderCacheManager& operator=(const ShaderCacheManager&) = delete;

        /**
         * Checks if any of the global shader cache parameters changed
         * (such as build mode, shader model, etc.). If changed deletes the whole shader cache
         * directory (including caches of previously compiled shaders) and creates a fresh new shader cache
         * directory with up to date global parameters file.
         *
         * @remark If no global shader cache metadata file existed it will create it.
         *
         * @return An error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> refreshShaderCache();

    protected:
        /**
         * Only used internally, use @ref create to create objects of this type.
         *
         * @param pRenderer Used renderer.
         */
        ShaderCacheManager(Renderer* pRenderer);

        /**
         * Makes sure that the cache was generated using the same parameters as the current renderer's state
         * and that the cache can be safely used. This function checks shader language specific parameters
         * that might have changed.
         *
         * @param cacheConfig Config that stores global cache parameters.
         *
         * @return Empty if cache can be safely used, otherwise reason why it's outdated and cache can't be
         * used.
         */
        [[nodiscard]] virtual std::optional<std::string>
        isLanguageSpecificGlobalCacheOutdated(const ConfigManager& cacheConfig) = 0;

        /**
         * Writes shader language specific parameters that affect shader cache into the specified config.
         *
         * @param cacheConfig Config that stores global cache parameters.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        writeLanguageSpecificParameters(ConfigManager& cacheConfig) = 0;

    private:
        /** Groups names (keys in TOML file) of shader cache parameters. */
        struct GlobalShaderCacheParameterNames {
            /** Build mode. */
            static constexpr std::string_view sIsReleaseBuild = "is_release_build";

            /** Renderer type. */
            static constexpr std::string_view sRenderer = "renderer";
        };

        /**
         * Creates a new render-specific cache manager.
         *
         * @param pRenderer Used renderer.
         *
         * @return Cache manager.
         */
        static std::unique_ptr<ShaderCacheManager> create(Renderer* pRenderer);

        /**
         * Deletes the current shader cache directory (if exists) and creates a new one with
         * a new config that stores renderer's parameters.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createNewShaderCache();

        /**
         * Makes sure that the cache was generated using the same parameters as the current renderer's state
         * and that the cache can be safely used.
         *
         * @param cacheConfig Config that stores global cache parameters.
         *
         * @return Empty if cache can be safely used, otherwise reason why it's outdated and cache can't be
         * used.
         */
        [[nodiscard]] std::optional<std::string>
        isGlobalShaderCacheOutdated(const ConfigManager& cacheConfig);

        /** Used renderer. */
        Renderer* const pRenderer = nullptr;

        /**
         * Name of the file used to store global shader cache information.
         * Global shader cache information is used to determine if the shader cache is valid
         * or not (needs to be recompiled or not).
         *
         * Starts with a dot on purpose (because no shader can start with a dot - reserved for internal use).
         */
        static constexpr std::string_view sGlobalShaderCacheInfoFileName = ".shader_cache.toml";
    };
}
