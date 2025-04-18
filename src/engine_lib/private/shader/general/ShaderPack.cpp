﻿#include "ShaderPack.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"
#include "shader/general/Shader.h"
#include "shader/general/ShaderFilesystemPaths.hpp"
#include "render/Renderer.h"
#include "shader/glsl/format/GlslVertexFormatDescription.h"
#include "render/vulkan/VulkanRenderer.h"

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
        auto pShaderPack = std::shared_ptr<ShaderPack>(
            new ShaderPack(shaderDescription.sShaderName, shaderDescription.shaderType));

        // Choose valid configurations depending on the shader type.
        const std::set<std::set<ShaderMacro>>* pMacroConfigurations = nullptr;
        switch (shaderDescription.shaderType) {
        case (ShaderType::VERTEX_SHADER):
            pMacroConfigurations = &ShaderMacroConfigurations::validVertexShaderMacroConfigurations;
            break;
        case (ShaderType::FRAGMENT_SHADER):
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

            // Add engine macros for this shader.
            addEngineMacrosToShaderDescription(currentShaderDescription, macros, pRenderer);

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
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
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
        case (ShaderType::FRAGMENT_SHADER):
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

            // Add engine macros for this shader.
            addEngineMacrosToShaderDescription(currentShaderDescription, macros, pRenderer);

            // Add hash of the configuration to the shader name for logging.
            const auto sConfigurationText = ShaderMacroConfigurations::convertConfigurationToText(macros);
            currentShaderDescription.sShaderName += sConfigurationText;

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

    std::shared_ptr<Shader> ShaderPack::getShader(const std::set<ShaderMacro>& shaderConfiguration) {
        std::scoped_lock guard(mtxInternalResources.first);

        // Find a shader which configuration is equal to the configuration we are looking for.
        auto it = mtxInternalResources.second.shadersInPack.find(shaderConfiguration);
        if (it == mtxInternalResources.second.shadersInPack.end()) [[unlikely]] {
            // Nothing found.
            Error error(std::format(
                "unable to find a shader in shader pack \"{}\" that matches the specified shader "
                "configuration: {}",
                sShaderName,
                formatShaderMacros(convertShaderMacrosToText(shaderConfiguration))));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return it->second;
    }

    std::string ShaderPack::getShaderName() const { return sShaderName; }

    ShaderType ShaderPack::getShaderType() { return shaderType; }

    std::pair<std::mutex, ShaderPack::InternalResources>* ShaderPack::getInternalResources() {
        return &mtxInternalResources;
    }

    void ShaderPack::addEngineMacrosToShaderDescription(
        ShaderDescription& description,
        const std::set<ShaderMacro>& shaderConfigurationMacros,
        Renderer* pRenderer) {
        // Specify configuration macros.
        auto vParameterNames = convertShaderMacrosToText(shaderConfigurationMacros);
        for (const auto& sParameter : vParameterNames) {
            description.definedShaderMacros[sParameter] = ""; // valueless macro
        }

        // See if we need to specify vertex format related macros.
        if (!description.vertexFormat.has_value()) {
            // Nothing more to do.
            return;
        }

        if (dynamic_cast<VulkanRenderer*>(pRenderer) == nullptr) {
            // HLSL shaders don't need more macros.
            return;
        }

        // Get layout macros.
        const auto pVertexFormat =
            GlslVertexFormatDescription::createDescription(description.vertexFormat.value());
        const auto vLayoutMacros = pVertexFormat->getVertexLayoutBindingIndexMacros();

        // Define layout macros.
        for (size_t i = 0; i < vLayoutMacros.size(); i++) {
            description.definedShaderMacros[vLayoutMacros[i]] = std::to_string(i);
        }
    }

    ShaderPack::ShaderPack(const std::string& sShaderName, ShaderType shaderType) {
        this->sShaderName = sShaderName;
        this->shaderType = shaderType;
    }
} // namespace ne
