﻿#include "GlslShader.h"

// Standard.
#include <fstream>
#include <climits>
#include <filesystem>

// Custom.
#include "io/Logger.h"
#include "misc/Globals.h"
#include "materials/ShaderFilesystemPaths.hpp"
#include "game/nodes/MeshNode.h"
#include "render/vulkan/VulkanRenderer.h"

namespace ne {
    GlslShader::GlslShader(
        Renderer* pRenderer,
        std::filesystem::path pathToCompiledShader,
        const std::string& sShaderName,
        ShaderType shaderType)
        : Shader(pRenderer, std::move(pathToCompiledShader), sShaderName, shaderType) {
        static_assert(
            sizeof(MeshVertex) == 32, // NOLINT: current size
            "`VkVertexInputAttributeDescription` needs to be updated");
    }

    std::variant<std::shared_ptr<Shader>, std::string, Error> GlslShader::compileShader(
        Renderer* pRenderer,
        const std::filesystem::path& cacheDirectory,
        const std::string& sConfiguration,
        const ShaderDescription& shaderDescription) {
        // Check that the renderer is Vulkan renderer.
        if (dynamic_cast<VulkanRenderer*>(pRenderer) == nullptr) [[unlikely]] {
            return Error("the specified renderer is not a Vulkan renderer");
        }

        // Read shader file.
        auto result = ShaderIncluder::parseFullSourceCode(shaderDescription.pathToShaderFile);
        if (std::holds_alternative<ShaderIncluder::Error>(result)) [[unlikely]] {
            return Error(fmt::format(
                "failed to parse shader source code, error: {}",
                convertShaderIncluderErrorToString(std::get<ShaderIncluder::Error>(result))));
        }
        const auto sFullShaderSourceCode = std::get<std::string>(std::move(result));

        // Prepare a compiler object.
        shaderc::Compiler compiler;
        shaderc::CompileOptions compileOptions;

        // Specify defined macros.
        for (const auto& sMacro : shaderDescription.vDefinedShaderMacros) {
            compileOptions.AddMacroDefinition(sMacro);
        }

        // Make warnings as errors.
        compileOptions.SetWarningsAsErrors();

        // Specify optimization level.
#if defined(DEBUG)
        compileOptions.SetGenerateDebugInfo();
        compileOptions.SetOptimizationLevel(shaderc_optimization_level_zero);
#else
        compileOptions.SetOptimizationLevel(shaderc_optimization_level_performance);
#endif

        // Prepare shader source file name for compilation.
        const auto sShaderSourceFileName = shaderDescription.pathToShaderFile.stem().string();

        // Compile the shader.
        const auto compilationResult = compiler.CompileGlslToSpv(
            sFullShaderSourceCode.data(),
            sFullShaderSourceCode.size(),
            convertShaderTypeToShadercShaderKind(shaderDescription.shaderType),
            sShaderSourceFileName.c_str(),
            shaderDescription.sShaderEntryFunctionName.c_str(),
            compileOptions);

        // Check if there were compilation warnings/errors.
        if (compilationResult.GetCompilationStatus() != shaderc_compilation_status_success) {
            return compilationResult.GetErrorMessage();
        }

        // Get compiled SPIR-V bytecode.
        using bytecode_element_type = uint32_t;
        std::vector<bytecode_element_type> vCompiledBytecode(
            compilationResult.cbegin(), compilationResult.cend());

        // Write shader bytecode to file.
        auto pathToCompiledShader = cacheDirectory / ShaderFilesystemPaths::getShaderCacheBaseFileName();
        pathToCompiledShader += sConfiguration;
        std::ofstream shaderCacheFile(pathToCompiledShader, std::ios::binary);
        if (!shaderCacheFile.is_open()) [[unlikely]] {
            return Error(fmt::format(
                "failed to open the path \"{}\" for writing to save shader bytecode",
                pathToCompiledShader.string()));
        }
        shaderCacheFile.write(
            reinterpret_cast<const char*>(vCompiledBytecode.data()),
            vCompiledBytecode.size() * sizeof(bytecode_element_type));
        shaderCacheFile.close();

        // Create shader instance.
        return std::make_shared<GlslShader>(
            pRenderer, pathToCompiledShader, shaderDescription.sShaderName, shaderDescription.shaderType);
    }

    bool GlslShader::releaseShaderDataFromMemoryIfLoaded() {
        std::scoped_lock guard(mtxSpirvBytecode.first);

        if (mtxSpirvBytecode.second.empty()) {
            return true;
        }

        mtxSpirvBytecode.second.clear();

        notifyShaderBytecodeReleasedFromMemory();

        return false;
    }

    std::optional<Error>
    GlslShader::saveAdditionalCompilationResultsInfo(ConfigManager& cacheMetadataConfigManager) {
        return {};
    }

    std::optional<Error> GlslShader::checkCachedAdditionalCompilationResultsInfo(
        ConfigManager& cacheMetadataConfigManager,
        std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
        return {};
    }

    std::variant<std::pair<std::recursive_mutex, std::vector<char>>*, Error>
    GlslShader::getShaderSpirvBytecode() {
        std::scoped_lock guard(mtxSpirvBytecode.first);

        if (mtxSpirvBytecode.second.empty()) {
            // Get path to compiled shader.
            auto pathResult = getPathToCompiledShader();
            if (std::holds_alternative<Error>(pathResult)) {
                auto err = std::get<Error>(std::move(pathResult));
                err.addEntry();
                return err;
            }
            const auto pathToCompiledShader = std::get<std::filesystem::path>(std::move(pathResult));

            // Open file.
            std::ifstream file(pathToCompiledShader, std::ios::ate | std::ios::binary);
            if (!file.is_open()) [[unlikely]] {
                return Error(fmt::format("failed to open the file \"{}\"", pathToCompiledShader.string()));
            }

            // Get file size.
            const auto iFileSize = file.tellg();
            file.seekg(0);

            // Reserve data.
            mtxSpirvBytecode.second.reserve(iFileSize);

            // Read file contents.
            file.read(mtxSpirvBytecode.second.data(), mtxSpirvBytecode.second.size());

            // Close file.
            file.close();

            notifyShaderBytecodeLoadedIntoMemory();
        }

        return &mtxSpirvBytecode;
    }

    shaderc_shader_kind GlslShader::convertShaderTypeToShadercShaderKind(ShaderType shaderType) {
        switch (shaderType) {
        case (ShaderType::VERTEX_SHADER): {
            return shaderc_glsl_vertex_shader;
            break;
        }
        case (ShaderType::PIXEL_SHADER): {
            return shaderc_glsl_fragment_shader;
            break;
        }
        case (ShaderType::COMPUTE_SHADER): {
            return shaderc_glsl_compute_shader;
            break;
        }
        }

        Error err("unhandled case");
        err.showError();
        throw std::runtime_error(err.getFullErrorMessage());
    }

    std::string GlslShader::convertShaderIncluderErrorToString(ShaderIncluder::Error error) {
        switch (error) {
        case (ShaderIncluder::Error::CANT_OPEN_FILE): {
            return "can't open the specified shader file or some included shader file";
            break;
        }
        case (ShaderIncluder::Error::MISSING_QUOTES): {
            return "the specified shader file or some included shader file has `#include` keyword with "
                   "missing double quotes";
            break;
        }
        case (ShaderIncluder::Error::NOTHING_AFTER_INCLUDE): {
            return "the specified shader file or some included shader file has `#include` keyword with "
                   "nothing after it";
            break;
        }
        case (ShaderIncluder::Error::NO_SPACE_AFTER_KEYWORD): {
            return "the specified shader file or some included shader file has `#include` keyword "
                   "without a space after it";
            break;
        }
        case (ShaderIncluder::Error::PATH_HAS_NO_PARENT_PATH): {
            return "the specified shader file or some included shader file has `#include` keyword "
                   "that points to a path that has no parent directory";
            break;
        }
        case (ShaderIncluder::Error::PATH_IS_NOT_A_FILE): {
            return "the specified shader path or some included shader path is not a file";
            break;
        }
        }

        Error err("unhandled case");
        err.showError();
        throw std::runtime_error(err.getFullErrorMessage());
    }

} // namespace ne
