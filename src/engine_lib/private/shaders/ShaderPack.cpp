#include "ShaderPack.h"

// STL.
#include <ranges>

// Custom.
#include "io/Logger.h"
#include "shaders/hlsl/HlslShader.h"
#include "render/directx/DirectXRenderer.h"
#include "shaders/IShader.h"
#include "shaders/ShaderFilesystemPaths.hpp"

namespace ne {
    std::variant<std::shared_ptr<ShaderPack>, Error> ShaderPack::createFromCache(
        IRenderer* pRenderer,
        const ShaderDescription& shaderDescription,
        std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
        cacheInvalidationReason = {};

        const auto pathToShaderDirectory =
            ShaderFilesystemPaths::getPathToShaderCacheDirectory() / shaderDescription.sShaderName;
        const auto pathToCompiledShader =
            pathToShaderDirectory / ShaderFilesystemPaths::getShaderCacheBaseFileName();

        const auto pShaderPack = std::shared_ptr<ShaderPack>(new ShaderPack(shaderDescription.sShaderName));

        // Configurations.
        const std::set<std::set<ShaderParameter>>* parameterCombinations;
        switch (shaderDescription.shaderType) {
        case (ShaderType::VERTEX_SHADER):
            parameterCombinations = &ShaderParameterConfigurations::validVertexShaderParameterConfigurations;
            break;
        case (ShaderType::PIXEL_SHADER):
            parameterCombinations = &ShaderParameterConfigurations::validPixelShaderParameterConfigurations;
            break;
        case (ShaderType::COMPUTE_SHADER):
            parameterCombinations = &ShaderParameterConfigurations::validComputeShaderParameterConfigurations;
            break;
        }

        for (const auto& parameters : *parameterCombinations) {
            auto currentShaderDescription = shaderDescription;

            // Add configuration macros.
            auto vParameterNames = shaderParametersToText(parameters);
            for (const auto& sParameter : vParameterNames) {
                currentShaderDescription.vDefinedShaderMacros.push_back(sParameter);
            }

            const auto sConfigurationText =
                ShaderParameterConfigurations::convertConfigurationToText(parameters);
            currentShaderDescription.sShaderName +=
                sConfigurationText; // add configuration to name for logging

            // Add configuration to filename so that all shader variants will be stored in different files.
            auto currentPathToCompiledShader = pathToCompiledShader;
            currentPathToCompiledShader += sConfigurationText;

            auto result = IShader::createFromCache(
                pRenderer,
                currentPathToCompiledShader,
                currentShaderDescription,
                shaderDescription.sShaderName,
                cacheInvalidationReason);
            if (std::holds_alternative<Error>(result)) {
                // Clear invalid cache.
                std::filesystem::remove_all(
                    ShaderFilesystemPaths::getPathToShaderCacheDirectory() / shaderDescription.sShaderName);

                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }

            pShaderPack->shaders[parameters] = std::get<std::shared_ptr<IShader>>(result);
        }

        Logger::get().info(
            std::format("successfully loaded shader \"{}\" from cache", shaderDescription.sShaderName), "");

        return pShaderPack;
    }

    std::variant<std::shared_ptr<ShaderPack>, std::string, Error>
    ShaderPack::compileShaderPack(IRenderer* pRenderer, const ShaderDescription& shaderDescription) {
        // Configurations.
        const std::set<std::set<ShaderParameter>>* parameterCombinations;
        switch (shaderDescription.shaderType) {
        case (ShaderType::VERTEX_SHADER):
            parameterCombinations = &ShaderParameterConfigurations::validVertexShaderParameterConfigurations;
            break;
        case (ShaderType::PIXEL_SHADER):
            parameterCombinations = &ShaderParameterConfigurations::validPixelShaderParameterConfigurations;
            break;
        case (ShaderType::COMPUTE_SHADER):
            parameterCombinations = &ShaderParameterConfigurations::validComputeShaderParameterConfigurations;
            break;
        }

        auto pShaderPack = std::shared_ptr<ShaderPack>(new ShaderPack(shaderDescription.sShaderName));
        for (const auto& parameters : *parameterCombinations) {
            auto currentShaderDescription = shaderDescription;

            // Add configuration macros.
            auto vParameterNames = shaderParametersToText(parameters);
            for (const auto& sParameter : vParameterNames) {
                currentShaderDescription.vDefinedShaderMacros.push_back(sParameter);
            }

            const auto sConfigurationText =
                ShaderParameterConfigurations::convertConfigurationToText(parameters);
            currentShaderDescription.sShaderName +=
                sConfigurationText; // add configuration to name for logging

            // Cache directory.
            auto currentPathToCompiledShader = ShaderFilesystemPaths::getPathToShaderCacheDirectory() /
                                               shaderDescription.sShaderName; // use non modified name here

            // Compile shader for this configuration.
            const auto result = IShader::compileShader(
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
    ShaderPack::changeConfiguration(const std::set<ShaderParameter>& configuration) {
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

    bool ShaderPack::releaseShaderPackDataFromMemoryIfLoaded(bool bLogOnlyErrors) {
        std::scoped_lock guard(mtxShaders);

        bool bResult = true;
        for (const auto& shader : shaders | std::views::values) {
            if (shader->releaseShaderDataFromMemoryIfLoaded(bLogOnlyErrors) == false) {
                bResult = false;
            }
        }

        return bResult;
    }

    std::string ShaderPack::getShaderName() const { return sShaderName; }

    ShaderType ShaderPack::getShaderType() {
        std::scoped_lock guard(mtxShaders);
        return shaders.begin()->second->getShaderType();
    }

    ShaderPack::ShaderPack(const std::string& sShaderName) { this->sShaderName = sShaderName; }
} // namespace ne
