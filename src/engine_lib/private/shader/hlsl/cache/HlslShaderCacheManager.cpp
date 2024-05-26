#include "HlslShaderCacheManager.h"

// Custom.
#include "shader/hlsl/HlslShader.h"

namespace ne {

    HlslShaderCacheManager::HlslShaderCacheManager(Renderer* pRenderer) : ShaderCacheManager(pRenderer) {}

    std::optional<std::string>
    HlslShaderCacheManager::isLanguageSpecificGlobalCacheOutdated(const ConfigManager& cacheConfig) {
        {
            // Read vertex shader model.
            const auto sOldVsModel = cacheConfig.getValue<std::string>(
                sHlslSectionName, GlobalShaderCacheParameterNames::sVsModel, "");

            // Compare.
            if (sOldVsModel != HlslShader::getVertexShaderModel()) {
                return "vertex shader model changed";
            }
        }

        {
            // Read pixel shader model.
            const auto sOldHlslPsModel = cacheConfig.getValue<std::string>(
                sHlslSectionName, GlobalShaderCacheParameterNames::sPsModel, "");

            // Compare.
            if (sOldHlslPsModel != HlslShader::getPixelShaderModel()) {
                return "pixel shader model changed";
            }
        }

        {
            // Read compute shader model.
            const auto sOldHlslCsModel = cacheConfig.getValue<std::string>(
                sHlslSectionName, GlobalShaderCacheParameterNames::sCsModel, "");

            // Compare.
            if (sOldHlslCsModel != HlslShader::getComputeShaderModel()) {
                return "compute shader model changed";
            }
        }

        {
            // Read compiler version.
            const auto sOldCompilerVersion = cacheConfig.getValue<std::string>(
                sHlslSectionName, GlobalShaderCacheParameterNames::sCompilerVersion, "");

            // Get compiler version.
            auto compilerVersionResult = HlslShader::getShaderCompilerVersion();
            if (std::holds_alternative<Error>(compilerVersionResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(compilerVersionResult));
                error.addCurrentLocationToErrorStack();

                // Log error here.
                Logger::get().error(error.getFullErrorMessage());

                // Mark as invalid.
                return "unable to get compiler version";
            }
            const auto sCompilerVersion = std::get<std::string>(std::move(compilerVersionResult));

            // Compare.
            if (sCompilerVersion != sOldCompilerVersion) {
                return "compiler version changed";
            }
        }

        return {};
    }

    std::optional<Error> HlslShaderCacheManager::writeLanguageSpecificParameters(ConfigManager& cacheConfig) {
        // Write shader model.
        cacheConfig.setValue<std::string>(
            sHlslSectionName, GlobalShaderCacheParameterNames::sVsModel, HlslShader::getVertexShaderModel());
        cacheConfig.setValue<std::string>(
            sHlslSectionName, GlobalShaderCacheParameterNames::sPsModel, HlslShader::getPixelShaderModel());
        cacheConfig.setValue<std::string>(
            sHlslSectionName, GlobalShaderCacheParameterNames::sCsModel, HlslShader::getComputeShaderModel());

        // Get compiler version.
        auto compilerVersionResult = HlslShader::getShaderCompilerVersion();
        if (std::holds_alternative<Error>(compilerVersionResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(compilerVersionResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto sCompilerVersion = std::get<std::string>(std::move(compilerVersionResult));

        // Write compiler version.
        cacheConfig.setValue<std::string>(
            sHlslSectionName, GlobalShaderCacheParameterNames::sCompilerVersion, sCompilerVersion);

        return {};
    }

}
