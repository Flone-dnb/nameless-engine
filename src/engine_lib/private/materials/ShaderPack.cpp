#include "ShaderPack.h"

// Standard.
#include <ranges>

// Custom.
#include "io/Logger.h"
#include "materials/Shader.h"
#include "materials/ShaderFilesystemPaths.hpp"
#include "render/Renderer.h"
#include "game/Game.h"
#include "game/Window.h"

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

        const auto pShaderPack = std::shared_ptr<ShaderPack>(
            new ShaderPack(shaderDescription.sShaderName, shaderDescription.shaderType));

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
                // Delete invalid cache.
                std::filesystem::remove_all(
                    ShaderFilesystemPaths::getPathToShaderCacheDirectory() / shaderDescription.sShaderName);

                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }

            pShaderPack->mtxShadersInPack.second[parameters] = std::get<std::shared_ptr<Shader>>(result);
        }

        Logger::get().info(
            fmt::format("successfully loaded shader \"{}\" from cache", shaderDescription.sShaderName),
            sShaderPackLogCategory);

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

        auto pShaderPack = std::shared_ptr<ShaderPack>(
            new ShaderPack(shaderDescription.sShaderName, shaderDescription.shaderType));
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
                pShaderPack->mtxShadersInPack.second[parameters] = std::get<std::shared_ptr<Shader>>(result);
            } else if (std::holds_alternative<std::string>(result)) {
                return std::get<std::string>(result);
            } else {
                return std::get<Error>(result);
            }
        }

        return pShaderPack;
    }

    bool ShaderPack::setConfiguration(const std::set<ShaderParameter>& configuration) {
        std::scoped_lock guard(mtxShadersInPack.first, mtxCurrentConfigurationShader.first);

        if (mtxCurrentConfigurationShader.second != nullptr) {
            if (this->configuration == configuration) {
                return false; // do nothing
            }

            // Try to release the old shader.
            mtxCurrentConfigurationShader.second->get()->releaseShaderDataFromMemoryIfLoaded();
            mtxCurrentConfigurationShader.second = nullptr;
        }

        this->configuration = configuration;

        // Find shader for the specified configuration.
        const auto it = mtxShadersInPack.second.find(configuration);
        if (it == mtxShadersInPack.second.end()) [[unlikely]] {
            return true; // failed to find
        }

        // Save found shader.
        mtxCurrentConfigurationShader.second = &it->second;

        return false;
    }

    bool ShaderPack::releaseShaderPackDataFromMemoryIfLoaded(bool bLogOnlyErrors) {
        std::scoped_lock guard(mtxShadersInPack.first);

        bool bResult = true;
        for (const auto& [key, shader] : mtxShadersInPack.second) {
            if (!shader->releaseShaderDataFromMemoryIfLoaded(bLogOnlyErrors)) {
                bResult = false;
            }
        }

        return bResult;
    }

    std::shared_ptr<Shader> ShaderPack::getShader() {
        std::scoped_lock guard(mtxCurrentConfigurationShader.first);

        // Make sure the configuration was set.
        if (mtxCurrentConfigurationShader.second == nullptr) [[unlikely]] {
            Error error(fmt::format(
                "configuration for the shader \"{}\" was not set yet but the shader is already requested",
                sShaderName));
            error.showError();
            throw std::runtime_error(error.getError());
        }

        return *mtxCurrentConfigurationShader.second;
    }

    std::string ShaderPack::getShaderName() const { return sShaderName; }

    ShaderType ShaderPack::getShaderType() { return shaderType; }

    ShaderPack::ShaderPack(const std::string& sShaderName, ShaderType shaderType) {
        this->sShaderName = sShaderName;
        this->shaderType = shaderType;

        mtxCurrentConfigurationShader.second = nullptr;
    }
} // namespace ne
