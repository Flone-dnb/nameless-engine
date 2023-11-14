#include "ShaderPack.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"
#include "shader/general/Shader.h"
#include "shader/general/ShaderFilesystemPaths.hpp"
#include "render/Renderer.h"

namespace ne {
    std::variant<std::shared_ptr<ShaderPack>, Error> ShaderPack::createFromCache(
        Renderer* pRenderer,
        const ShaderDescription& shaderDescription,
        std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
        // Reset received cache invalidation reason (just in case it contains a value).
        cacheInvalidationReason = {};

        // Prepare paths to shader.
        const auto pathToShaderDirectory =
            ShaderFilesystemPaths::getPathToShaderCacheDirectory() / shaderDescription.sShaderName;
        const auto pathToCompiledShader =
            pathToShaderDirectory / ShaderFilesystemPaths::getShaderCacheBaseFileName();

        // Create an empty shader pack.
        const auto pShaderPack = std::shared_ptr<ShaderPack>(
            new ShaderPack(shaderDescription.sShaderName, shaderDescription.shaderType));

        // Choose valid configurations depending on the shader type.
        const std::set<std::set<ShaderMacro>>* pMacroConfigurations = nullptr;
        switch (shaderDescription.shaderType) {
        case (ShaderType::VERTEX_SHADER):
            pMacroConfigurations = &ShaderMacroConfigurations::validVertexShaderMacroConfigurations;
            break;
        case (ShaderType::PIXEL_SHADER):
            pMacroConfigurations = &ShaderMacroConfigurations::validPixelShaderMacroConfigurations;
            break;
        case (ShaderType::COMPUTE_SHADER):
            pMacroConfigurations = &ShaderMacroConfigurations::validComputeShaderMacroConfigurations;
            break;
        }

        std::scoped_lock resourcesGuard(pShaderPack->mtxInternalResources.first);

        // Prepare a shader per macro configuration and add it to the shader pack.
        for (const auto& macros : *pMacroConfigurations) {
            // Prepare shader description.
            auto currentShaderDescription = shaderDescription;

            // Specify defined macros for this shader.
            auto vParameterNames = convertShaderMacrosToText(macros);
            for (const auto& sParameter : vParameterNames) {
                currentShaderDescription.definedShaderMacros[sParameter] = ""; // valueless macro
            }

            // Add hash of the configuration to the shader name for logging.
            const auto sConfigurationText = ShaderMacroConfigurations::convertConfigurationToText(macros);
            currentShaderDescription.sShaderName += sConfigurationText;

            // Add hash of the configuration to the compiled bytecode file name
            // so that all shader variants will be stored in different files.
            auto currentPathToCompiledShaderBytecode = pathToCompiledShader;
            currentPathToCompiledShaderBytecode += sConfigurationText;

            // Try to load the shader from cache.
            auto result = Shader::createFromCache(
                pRenderer,
                currentPathToCompiledShaderBytecode,
                currentShaderDescription,
                shaderDescription.sShaderName,
                cacheInvalidationReason);
            if (std::holds_alternative<Error>(result)) {
                // Shader cache is corrupted or invalid. Delete invalid cache directory.
                std::filesystem::remove_all(
                    ShaderFilesystemPaths::getPathToShaderCacheDirectory() / shaderDescription.sShaderName);

                // Return error that specifies that cache is invalid.
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                return err;
            }

            // Save loaded shader to shader pack.
            pShaderPack->mtxInternalResources.second.shadersInPack[macros] =
                std::get<std::shared_ptr<Shader>>(result);
        }

        // Log finish.
        Logger::get().info(
            std::format("successfully loaded shader \"{}\" from cache", shaderDescription.sShaderName));

        return pShaderPack;
    }

    std::variant<std::shared_ptr<ShaderPack>, std::string, Error>
    ShaderPack::compileShaderPack(Renderer* pRenderer, const ShaderDescription& shaderDescription) {
        // Choose a valid configurations depending on our shader type.
        const std::set<std::set<ShaderMacro>>* pMacroConfigurations;
        switch (shaderDescription.shaderType) {
        case (ShaderType::VERTEX_SHADER):
            pMacroConfigurations = &ShaderMacroConfigurations::validVertexShaderMacroConfigurations;
            break;
        case (ShaderType::PIXEL_SHADER):
            pMacroConfigurations = &ShaderMacroConfigurations::validPixelShaderMacroConfigurations;
            break;
        case (ShaderType::COMPUTE_SHADER):
            pMacroConfigurations = &ShaderMacroConfigurations::validComputeShaderMacroConfigurations;
            break;
        }

        // Create pack.
        auto pShaderPack = std::shared_ptr<ShaderPack>(
            new ShaderPack(shaderDescription.sShaderName, shaderDescription.shaderType));

        // Compile a shader per macro configuration and add it to pack.
        for (const auto& macros : *pMacroConfigurations) {
            // Prepare shader description.
            auto currentShaderDescription = shaderDescription;

            // Add configuration macros to description.
            auto vParameterNames = convertShaderMacrosToText(macros);
            for (const auto& sParameter : vParameterNames) {
                currentShaderDescription.definedShaderMacros[sParameter] = ""; // valueless macro
            }

            const auto sConfigurationText = ShaderMacroConfigurations::convertConfigurationToText(macros);
            currentShaderDescription.sShaderName +=
                sConfigurationText; // add configuration to name for logging

            // Prepare path to cache directory.
            auto currentPathToCompiledShader = ShaderFilesystemPaths::getPathToShaderCacheDirectory() /
                                               shaderDescription.sShaderName; // use non modified name here

            // Compile shader for this configuration.
            const auto result = Shader::compileShader(
                pRenderer, currentPathToCompiledShader, sConfigurationText, currentShaderDescription);

            if (!std::holds_alternative<std::shared_ptr<Shader>>(result)) {
                // Delete any created files.
                std::filesystem::remove_all(
                    ShaderFilesystemPaths::getPathToShaderCacheDirectory() / shaderDescription.sShaderName);
            }

            if (std::holds_alternative<std::shared_ptr<Shader>>(result)) {
                pShaderPack->mtxInternalResources.second.shadersInPack[macros] =
                    std::get<std::shared_ptr<Shader>>(result);
            } else if (std::holds_alternative<std::string>(result)) {
                return std::get<std::string>(result);
            } else {
                return std::get<Error>(result);
            }
        }

        return pShaderPack;
    }

    void ShaderPack::setRendererConfiguration(const std::set<ShaderMacro>& renderConfiguration) {
        std::scoped_lock configurationGuard(mtxInternalResources.first);

        mtxInternalResources.second.bIsRenderConfigurationSet = true;

        if (mtxInternalResources.second.renderConfiguration == renderConfiguration) {
            return; // do nothing
        }

        // Try to release previously used (old) shaders.
        for (const auto& [macros, pShader] : mtxInternalResources.second.shadersInPack) {
            pShader->releaseShaderDataFromMemoryIfLoaded();
        }

        mtxInternalResources.second.renderConfiguration = renderConfiguration;
    }

    bool ShaderPack::releaseShaderPackDataFromMemoryIfLoaded() {
        std::scoped_lock guard(mtxInternalResources.first);

        bool bAtLeastOneWasReleased = false;
        for (const auto& [macros, pShader] : mtxInternalResources.second.shadersInPack) {
            if (!pShader->releaseShaderDataFromMemoryIfLoaded()) {
                bAtLeastOneWasReleased = true;
            }
        }

        return !bAtLeastOneWasReleased;
    }

    std::shared_ptr<Shader> ShaderPack::getShader(
        const std::set<ShaderMacro>& additionalConfiguration,
        std::set<ShaderMacro>& fullShaderConfiguration) {
        std::scoped_lock guard(mtxInternalResources.first);

        // Make sure the renderer's configuration was previously set.
        if (!mtxInternalResources.second.bIsRenderConfigurationSet) [[unlikely]] {
            Error error(std::format(
                "render configuration for the shader \"{}\" was not set yet but the shader was already "
                "requested",
                sShaderName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Combine renderer's shader configuration and the specified one.
        auto targetShaderConfiguration = additionalConfiguration;
        for (const auto& macro : mtxInternalResources.second.renderConfiguration) {
#if defined(DEBUG)
            // Check if something is wrong and additional configuration has macros that renderer defines.
            auto it = additionalConfiguration.find(macro);
            if (it != additionalConfiguration.end()) [[unlikely]] {
                // Unexpected, potential error somewhere else.
                Error error(std::format(
                    "shader macro \"{}\" of the specified additional shader configuration "
                    "is already defined by the renderer",
                    convertShaderMacrosToText({macro})[0]));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            // See if this macro should be considered (valid) in this configuration.
            if (!ShaderMacroConfigurations::isMacroShouldBeConsideredInConfiguration(
                    macro, additionalConfiguration)) {
                continue;
            }

            targetShaderConfiguration.insert(macro);
        }

        // Find a shader which configuration is equal to the configuration we are looking for.
        auto it = mtxInternalResources.second.shadersInPack.find(targetShaderConfiguration);
        if (it == mtxInternalResources.second.shadersInPack.end()) [[unlikely]] {
            // Nothing found.
            Error error(std::format(
                "unable to find a shader in shader pack \"{}\" that matches the specified shader "
                "configuration: {}",
                sShaderName,
                formatShaderMacros(convertShaderMacrosToText(targetShaderConfiguration))));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        fullShaderConfiguration = targetShaderConfiguration;
        return it->second;
    }

    std::string ShaderPack::getShaderName() const { return sShaderName; }

    ShaderType ShaderPack::getShaderType() { return shaderType; }

    std::pair<std::mutex, ShaderPack::InternalResources>* ShaderPack::getInternalResources() {
        return &mtxInternalResources;
    }

    ShaderPack::ShaderPack(const std::string& sShaderName, ShaderType shaderType) {
        this->sShaderName = sShaderName;
        this->shaderType = shaderType;
    }
} // namespace ne
