#pragma once

// Custom.
#include "shader/general/cache/ShaderCacheManager.h"

namespace ne {
    class Renderer;

    /** Responsible for validating HLSL shader cache, reading and updating the cache. */
    class HlslShaderCacheManager : public ShaderCacheManager {
        // Only shader manager is expected to create this manager.
        friend class ShaderCacheManager;

    public:
        virtual ~HlslShaderCacheManager() override = default;

        HlslShaderCacheManager(const HlslShaderCacheManager&) = delete;
        HlslShaderCacheManager& operator=(const HlslShaderCacheManager&) = delete;

    protected:
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
        isLanguageSpecificGlobalCacheOutdated(const ConfigManager& cacheConfig) override;

        /**
         * Writes shader language specific parameters that affect shader cache into the specified config.
         *
         * @param cacheConfig Config that stores global cache parameters.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        writeLanguageSpecificParameters(ConfigManager& cacheConfig) override;

    private:
        /** Groups names (keys in TOML file) of shader cache parameters. */
        struct GlobalShaderCacheParameterNames {
            /** Vertex shader model. */
            static constexpr std::string_view sVsModel = "vs";

            /** Pixel shader model. */
            static constexpr std::string_view sPsModel = "ps";

            /** Compute shader model. */
            static constexpr std::string_view sCsModel = "cs";

            /** Compiler version. */
            static constexpr std::string_view sCompilerVersion = "compiler_version";
        };

        /**
         * Constructs a new manager.
         *
         * @param pRenderer Used renderer.
         */
        HlslShaderCacheManager(Renderer* pRenderer);

        /** Name of the section in TOML file where all HLSL parameters are stored. */
        static constexpr std::string_view sHlslSectionName = "hlsl";
    };
}
