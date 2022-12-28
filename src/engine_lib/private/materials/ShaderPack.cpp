#include "ShaderPack.h"

// Standard.
#include <ranges>

// Custom.
#include "io/Logger.h"
#include "materials/Shader.h"
#include "materials/ShaderFilesystemPaths.hpp"
#include "render/Renderer.h"

// External.
#include "fmt/core.h"

namespace ne {
    std::variant<std::shared_ptr<ShaderPack>, Error> ShaderPack::createFromCache(
        Renderer* pRenderer,
        const ShaderDescription& shaderDescription,
        std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
        cacheInvalidationReason = {};

        const auto pathToShaderDirectory =
            ShaderFilesystemPaths::getPathToShaderCacheDirectory() / shaderDescription.sShaderName;
        const auto pathToCompiledShader =
            pathToShaderDirectory / ShaderFilesystemPaths::getShaderCacheBaseFileName();

        const auto pShaderPack = std::shared_ptr<ShaderPack>(new ShaderPack(shaderDescription.sShaderName));

        // Configurations.
        const std::set<std::set<ShaderParameter>>* pParameterCombinations;
        switch (shaderDescription.shaderType) {
        case (ShaderType::VERTEX_SHADER):
            pParameterCombinations = &ShaderParameterConfigurations::validVertexShaderParameterConfigurations;
            break;
        case (ShaderType::PIXEL_SHADER):
            pParameterCombinations = &ShaderParameterConfigurations::validPixelShaderParameterConfigurations;
            break;
        case (ShaderType::COMPUTE_SHADER):
            pParameterCombinations =
                &ShaderParameterConfigurations::validComputeShaderParameterConfigurations;
            break;
        }

        for (const auto& parameters : *pParameterCombinations) {
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

            auto result = Shader::createFromCache(
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

            pShaderPack->shaders[parameters] = std::get<std::shared_ptr<Shader>>(result);
        }

        Logger::get().info(
            fmt::format("successfully loaded shader \"{}\" from cache", shaderDescription.sShaderName), "");

        return pShaderPack;
    }

    std::variant<std::shared_ptr<ShaderPack>, std::string, Error>
    ShaderPack::compileShaderPack(Renderer* pRenderer, const ShaderDescription& shaderDescription) {
        // Configurations.
        const std::set<std::set<ShaderParameter>>* pParameterCombinations;
        switch (shaderDescription.shaderType) {
        case (ShaderType::VERTEX_SHADER):
            pParameterCombinations = &ShaderParameterConfigurations::validVertexShaderParameterConfigurations;
            break;
        case (ShaderType::PIXEL_SHADER):
            pParameterCombinations = &ShaderParameterConfigurations::validPixelShaderParameterConfigurations;
            break;
        case (ShaderType::COMPUTE_SHADER):
            pParameterCombinations =
                &ShaderParameterConfigurations::validComputeShaderParameterConfigurations;
            break;
        }

        auto pShaderPack = std::shared_ptr<ShaderPack>(new ShaderPack(shaderDescription.sShaderName));
        for (const auto& parameters : *pParameterCombinations) {
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
            const auto result = Shader::compileShader(
                pRenderer, currentPathToCompiledShader, sConfigurationText, currentShaderDescription);
            if (std::holds_alternative<std::shared_ptr<Shader>>(result)) {
                pShaderPack->shaders[parameters] = std::get<std::shared_ptr<Shader>>(result);
            } else if (std::holds_alternative<std::string>(result)) {
                return std::get<std::string>(result);
            } else {
                return std::get<Error>(result);
            }
        }

        return pShaderPack;
    }

    std::shared_ptr<Shader> ShaderPack::changeConfiguration(const std::set<ShaderParameter>& configuration) {
        std::scoped_lock guard(mtxShaders);

        if (pCurrentConfigurationShader != nullptr) {
            if (this->configuration == configuration) {
                return *pCurrentConfigurationShader;
            }

            pCurrentConfigurationShader->get()->releaseShaderDataFromMemoryIfLoaded();
            pCurrentConfigurationShader = nullptr;
        }

        this->configuration = configuration;

        const auto it = shaders.find(configuration);
        if (it == shaders.end()) {
            return nullptr;
        }

        pCurrentConfigurationShader = &it->second;
        return it->second;
    }

    bool ShaderPack::releaseShaderPackDataFromMemoryIfLoaded(bool bLogOnlyErrors) {
        std::scoped_lock guard(mtxShaders);

        bool bResult = true;
        for (const auto& [key, shader] : shaders) {
            if (!shader->releaseShaderDataFromMemoryIfLoaded(bLogOnlyErrors)) {
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
