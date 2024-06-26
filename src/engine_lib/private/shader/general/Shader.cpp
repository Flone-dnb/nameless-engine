﻿#include "Shader.h"

// Standard.
#include <format>

// Custom.
#include "misc/Error.h"
#include "render/Renderer.h"
#include "shader/general/ShaderFilesystemPaths.hpp"
#if defined(WIN32)
#include "shader/hlsl/HlslShader.h"
#include "render/directx/DirectXRenderer.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "shader/glsl/GlslShader.h"

/** Total amount of shader blobs loaded into the memory. */
static std::atomic<size_t> iTotalShaderInMemoryCount{0};

namespace ne {
    Shader::Shader(
        Renderer* pRenderer,
        std::filesystem::path pathToCompiledShader,
        const std::string& sShaderName,
        ShaderType shaderType,
        std::optional<VertexFormat> vertexFormat)
        : pRenderer(pRenderer), optionalVertexFormat(vertexFormat), sShaderName(std::move(sShaderName)),
          shaderType(shaderType), pathToCompiledShader(std::move(pathToCompiledShader)) {}

    size_t Shader::getCurrentAmountOfShadersInMemory() { return iTotalShaderInMemoryCount.load(); }

    void Shader::notifyShaderBytecodeLoadedIntoMemory() { iTotalShaderInMemoryCount.fetch_add(1); }

    void Shader::notifyShaderBytecodeReleasedFromMemory() {
        const auto iPreviouslyShadersInMemoryCount = iTotalShaderInMemoryCount.fetch_sub(1);

        // Self check:
        if (iPreviouslyShadersInMemoryCount == 0) [[unlikely]] {
            Logger::get().error(
                "detected shader load/release notify mismatch, shaders loaded in the memory just "
                "went below 0");
        }
    }

    std::variant<std::shared_ptr<Shader>, std::string, Error> Shader::compileRenderDependentShader(
        Renderer* pRenderer,
        const std::filesystem::path& shaderCacheDirectory,
        const std::string& sConfiguration,
        const ShaderDescription& shaderDescription) {
        // Make sure the specified file exists.
        if (!std::filesystem::exists(shaderDescription.pathToShaderFile)) [[unlikely]] {
            return Error(std::format(
                "the specified shader file {} does not exist", shaderDescription.pathToShaderFile.string()));
        }

        // Make sure the specified path is a file.
        if (std::filesystem::is_directory(shaderDescription.pathToShaderFile)) [[unlikely]] {
            return Error(std::format(
                "the specified shader path {} is not a file", shaderDescription.pathToShaderFile.string()));
        }

        // Create shader cache directory if needed.
        if (!std::filesystem::exists(shaderCacheDirectory)) {
            std::filesystem::create_directory(shaderCacheDirectory);
        }

#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            return HlslShader::compileShader(
                pRenderer, shaderCacheDirectory, sConfiguration, shaderDescription);
        }
#endif
        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            return GlslShader::compileShader(
                pRenderer, shaderCacheDirectory, sConfiguration, shaderDescription);
        }

        return Error("unsupported renderer");
    }

    std::variant<std::shared_ptr<Shader>, std::string, Error> Shader::compileShader(
        Renderer* pRenderer,
        const std::filesystem::path& shaderCacheDirectory,
        const std::string& sConfiguration,
        const ShaderDescription& shaderDescription) {
        // Create shader directory if needed.
        if (!std::filesystem::exists(shaderCacheDirectory)) {
            std::filesystem::create_directories(shaderCacheDirectory);
        }

        // Compile shader.
        auto result =
            compileRenderDependentShader(pRenderer, shaderCacheDirectory, sConfiguration, shaderDescription);

        // Exit now if there was an error.
        if (!std::holds_alternative<std::shared_ptr<Shader>>(result)) {
            return result;
        }

        // Success. Cache shader's description on disk.
        // Prepare path to the file.
        auto shaderCacheConfigurationPath =
            shaderCacheDirectory / ShaderFilesystemPaths::getShaderCacheBaseFileName();
        shaderCacheConfigurationPath += sConfiguration;

        // Save shader description to cache metadata file.
        ConfigManager configManager;
        configManager.setValue<ShaderDescription>(
            "", ShaderDescription::getConfigurationFileSectionName(), shaderDescription);

        // Get path to compiled shader bytecode file.
        const auto pCompiledShader = std::get<std::shared_ptr<Shader>>(result);
        auto pathToCompiledShaderResult = pCompiledShader->getPathToCompiledShader();
        if (std::holds_alternative<Error>(pathToCompiledShaderResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(pathToCompiledShaderResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto pathToCompiledShader =
            std::get<std::filesystem::path>(std::move(pathToCompiledShaderResult));

        // Calculate hash of compiled shader bytecode file.
        const auto sCompiledFileHash =
            ShaderDescription::getFileHash(pathToCompiledShader, shaderDescription.sShaderName);
        if (sCompiledFileHash.empty()) [[unlikely]] {
            return Error(std::format(
                "failed to calculate hash of compiled shader bytecode at \"{}\"",
                pathToCompiledShader.string()));
        }

        // Save hash of the compiled bytecode to later test during cache validation.
        configManager.setValue<std::string>("", sCompiledBytecodeHashKeyName, sCompiledFileHash);

        // Save other additional information.
        auto optionalError = pCompiledShader->saveAdditionalCompilationResultsInfo(configManager);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Save cache metadata file.
        optionalError = configManager.saveFile(shaderCacheConfigurationPath, false);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return result;
    }

    std::variant<std::shared_ptr<Shader>, Error> Shader::createFromCache(
        Renderer* pRenderer,
        const std::filesystem::path& pathToCompiledShaderBytecode,
        ShaderDescription& shaderDescription,
        const std::string& sShaderNameWithoutConfiguration,
        std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
        cacheInvalidationReason = {};
        // Make sure the specified path to compiled shader exists.
        if (!std::filesystem::exists(pathToCompiledShaderBytecode)) {
            return Error(std::format(
                "the specified path to compiled shader bytecode \"{}\" does not exist",
                pathToCompiledShaderBytecode.string()));
        }

        // Prepare path to the file that stores metadata about this shader's cache.
        const auto shaderCacheConfigurationPath =
            pathToCompiledShaderBytecode.string() + ConfigManager::getConfigFormatExtension();

        // Make sure the metadata file exists.
        if (!std::filesystem::exists(shaderCacheConfigurationPath)) [[unlikely]] {
            return Error(std::format(
                "cache metadata of the specified shader does not exist", shaderDescription.sShaderName));
        }

        // Read shader cache metadata file from disk.
        ConfigManager configManager;
        auto optionalError = configManager.loadFile(shaderCacheConfigurationPath);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Generate shader description that was specified when this shader was compiled.
        auto cachedShaderDescription = configManager.getValue<ShaderDescription>(
            "", ShaderDescription::getConfigurationFileSectionName(), ShaderDescription());

        // Check if the current shader description is equal to the shader description when
        // this shader was compiled.
        const auto reason = shaderDescription.isSerializableDataEqual(cachedShaderDescription);
        if (reason.has_value()) {
            // Something has changed, cache is no longer valid.
            Error err(std::format(
                "invalidated cache for shader \"{}\" (reason: {})",
                sShaderNameWithoutConfiguration,
                ShaderCacheInvalidationReasonDescription::getDescription(reason.value())));
            cacheInvalidationReason = reason.value();
            return err;
        }

        // Now check if bytecode and other compilation results (from the old compilation) are the same.

        // Calculate hash of existing shader bytecode file that was previously compiled.
        const auto sCompiledFileHash =
            ShaderDescription::getFileHash(pathToCompiledShaderBytecode, shaderDescription.sShaderName);
        if (sCompiledFileHash.empty()) [[unlikely]] {
            return Error(std::format(
                "failed to calculate hash of compiled shader bytecode at \"{}\"",
                pathToCompiledShaderBytecode.string()));
        }

        // Read hash of the compiled bytecode from cache metadata file.
        const auto sCachedCompiledFileHash =
            configManager.getValue<std::string>("", sCompiledBytecodeHashKeyName, "");

        // Make sure compiled bytecode file is the same.
        if (sCompiledFileHash != sCachedCompiledFileHash) [[unlikely]] {
            // File was changed, cache is no longer valid.
            cacheInvalidationReason = ShaderCacheInvalidationReason::COMPILED_BINARY_CHANGED;
            Error err(std::format(
                "invalidated cache for shader \"{}\" (reason: {})",
                sShaderNameWithoutConfiguration,
                ShaderCacheInvalidationReasonDescription::getDescription(cacheInvalidationReason.value())));
            return err;
        }

        // Create a new shader from cache.
        auto result = createRenderDependentShaderFromCache(
            pRenderer,
            shaderDescription.pathToShaderFile,
            pathToCompiledShaderBytecode,
            shaderDescription.sShaderName,
            shaderDescription.shaderType,
            shaderDescription.vertexFormat);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pShader = std::get<std::shared_ptr<Shader>>(std::move(result));

        // Check if other compilation results are valid.
        optionalError =
            pShader->checkCachedAdditionalCompilationResultsInfo(configManager, cacheInvalidationReason);
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Check if cache was invalidated.
        if (cacheInvalidationReason.has_value()) {
            Error err(std::format(
                "invalidated cache for shader \"{}\" (reason: {})",
                sShaderNameWithoutConfiguration,
                ShaderCacheInvalidationReasonDescription::getDescription(cacheInvalidationReason.value())));
            return err;
        }

        return pShader;
    }

    std::variant<std::shared_ptr<Shader>, Error> Shader::createRenderDependentShaderFromCache(
        Renderer* pRenderer,
        const std::filesystem::path& pathToSourceShaderFile,
        const std::filesystem::path& pathToCompiledShaderBytecode,
        const std::string& sShaderName,
        ShaderType shaderType,
        std::optional<VertexFormat> vertexFormat) {
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            // Calculate source file hash so that we could determine what pixel/fragment/vertex shaders
            // were compiled from the same file.
            const auto sSourceFileHash = ShaderDescription::getFileHash(pathToSourceShaderFile, sShaderName);
            if (sSourceFileHash.empty()) {
                return Error(std::format(
                    "unable to calculate shader source file hash (shader path: \"{}\")",
                    pathToSourceShaderFile.string()));
            }

            return std::make_shared<HlslShader>(
                pRenderer,
                pathToCompiledShaderBytecode,
                sShaderName,
                shaderType,
                vertexFormat,
                sSourceFileHash);
        }
#endif
        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            return std::make_shared<GlslShader>(
                pRenderer, pathToCompiledShaderBytecode, sShaderName, shaderType, vertexFormat);
        }

        return Error("unsupported renderer");
    }

    std::string Shader::getShaderName() const { return sShaderName; }

    std::variant<std::filesystem::path, Error> Shader::getPathToCompiledShader() {
        if (!std::filesystem::exists(pathToCompiledShader)) [[unlikely]] {
            Error error(std::format(
                "path to compiled shader \"{}\" no longer exists", pathToCompiledShader.string()));
            return error;
        }
        return pathToCompiledShader;
    }

    std::optional<VertexFormat> Shader::getVertexFormat() const { return optionalVertexFormat; }

    Renderer* Shader::getRenderer() const { return pRenderer; }

    ShaderType Shader::getShaderType() const { return shaderType; }
} // namespace ne
