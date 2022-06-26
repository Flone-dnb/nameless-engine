#include "HlslShaderPack.h"

// STL.
#include <ranges>

namespace ne {
    HlslShaderPack::HlslShaderPack(
        IRenderer* pRenderer,
        const std::filesystem::path& pathToCompiledShader,
        const std::string& sShaderName,
        ShaderType shaderType)
        : IShaderPack(sShaderName) {
        const auto& parameterCombinations = shaderType == ShaderType::VERTEX_SHADER
                                                ? DirectXEngineShaders::validVertexShaderParameterCombinations
                                                : DirectXEngineShaders::validPixelShaderParameterCombinations;
        for (const auto& parameters : parameterCombinations) {
            const auto sConfigurationText = convertConfigurationToText(parameters);
            const auto sCurrentShaderName =
                sShaderName + sConfigurationText; // add configuration to name for logging

            // Add configuration to filename so that all shader variants will be stored in different files.
            auto currentPathToCompiledShader = pathToCompiledShader;
            currentPathToCompiledShader += sConfigurationText;

            const auto pShader = std::make_shared<HlslShader>(
                pRenderer, currentPathToCompiledShader, sCurrentShaderName, shaderType);
            shaders[parameters] = pShader;
        }
    }

    std::variant<std::shared_ptr<IShaderPack>, std::string, Error>
    HlslShaderPack::compileShader(IRenderer* pRenderer, const ShaderDescription& shaderDescription) {
        const auto& parameterCombinations = shaderDescription.shaderType == ShaderType::VERTEX_SHADER
                                                ? DirectXEngineShaders::validVertexShaderParameterCombinations
                                                : DirectXEngineShaders::validPixelShaderParameterCombinations;

        auto pShaderPack = std::shared_ptr<HlslShaderPack>(new HlslShaderPack(shaderDescription.sShaderName));
        for (const auto& parameters : parameterCombinations) {
            auto currentShaderDescription = shaderDescription;

            // Add configuration macros.
            auto vParameterNames = shaderParametersToText(parameters);
            for (const auto& sParameter : vParameterNames) {
                currentShaderDescription.vDefinedShaderMacros.push_back(sParameter);
            }

            const auto sConfigurationText = convertConfigurationToText(parameters);
            currentShaderDescription.sShaderName +=
                sConfigurationText; // add configuration to name for logging

            // Cache directory.
            auto currentPathToCompiledShader = IShader::getPathToShaderCacheDirectory() /
                                               shaderDescription.sShaderName; // use non modified name here

            // Compile shader for this configuration.
            const auto result = HlslShader::compileShader(
                pRenderer, currentPathToCompiledShader, sConfigurationText, currentShaderDescription);
            if (std::holds_alternative<std::shared_ptr<IShader>>(result)) {
                pShaderPack->shaders[parameters] = std::get<std::shared_ptr<IShader>>(result);
            } else if (std::holds_alternative<std::string>(result)) {
                return std::get<std::string>(result);
            } else {
                return std::get<Error>(result);
            }
        }

        return pShaderPack;
    }

    std::optional<std::shared_ptr<IShader>>
    HlslShaderPack::changeConfiguration(const std::set<DirectXShaderParameter>& configuration) {
        std::scoped_lock guard(mtxShaders);

        if (previouslyRequestedShader.has_value()) {
            previouslyRequestedShader.value()->get()->releaseShaderDataFromMemoryIfLoaded();
            previouslyRequestedShader = {};
        }

        const auto it = shaders.find(configuration);
        if (it == shaders.end()) {
            return {};
        }

        previouslyRequestedShader = &it->second;
        return it->second;
    }

    ShaderType HlslShaderPack::getShaderType() {
        std::scoped_lock guard(mtxShaders);
        return shaders.begin()->second->getShaderType();
    }

    std::optional<Error> HlslShaderPack::testIfShaderCacheIsCorrupted() {
        std::scoped_lock guard(mtxShaders);

        std::optional<Error> err = {};
        for (const auto& shader : shaders | std::views::values) {
            auto result = shader->testIfShaderCacheIsCorrupted();
            if (result.has_value()) {
                err = result;
                break;
            }
        }

        return err;
    }

    bool HlslShaderPack::releaseShaderPackDataFromMemoryIfLoaded(bool bLogOnlyErrors) {
        std::scoped_lock guard(mtxShaders);

        bool bResult = true;
        for (const auto& shader : shaders | std::views::values) {
            if (shader->releaseShaderDataFromMemoryIfLoaded(bLogOnlyErrors) == false) {
                bResult = false;
            }
        }

        return bResult;
    }

    HlslShaderPack::HlslShaderPack(const std::string& sShaderName) : IShaderPack(sShaderName) {}
} // namespace ne
