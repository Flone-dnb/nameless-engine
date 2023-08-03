#include "GlslShader.h"

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
        ShaderType shaderType,
        const std::string& sShaderEntryFunctionName)
        : Shader(pRenderer, std::move(pathToCompiledShader), sShaderName, shaderType) {
        // Make sure vertex attributes don't need update.
        static_assert(
            sizeof(MeshVertex) == 32, // NOLINT: current size
            "`getVertexAttributeDescriptions` needs to be updated");

        // Save entry function name.
        this->sShaderEntryFunctionName = sShaderEntryFunctionName;
    }

    VkVertexInputBindingDescription GlslShader::getVertexBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = iVertexBindingIndex;
        bindingDescription.stride = sizeof(MeshVertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    std::array<VkVertexInputAttributeDescription, 3> GlslShader::getVertexAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> vAttributeDescriptions{};

        // Prepare some constants.
        constexpr VkFormat vec3Format = VK_FORMAT_R32G32B32_SFLOAT;
        constexpr VkFormat vec2Format = VK_FORMAT_R32G32_SFLOAT;
        constexpr uint32_t iPositionAttributeOffset = 0;
        constexpr uint32_t iNormalAttributeOffset = 1;
        constexpr uint32_t iUvAttributeOffset = 2;

        // Describe position attribute.
        auto& positionAttribute = vAttributeDescriptions[iPositionAttributeOffset];
        positionAttribute.binding = iVertexBindingIndex;
        positionAttribute.location = iPositionAttributeOffset;
        positionAttribute.format = vec3Format;
        positionAttribute.offset = offsetof(MeshVertex, position);

        // Describe normal attribute.
        auto& normalAttribute = vAttributeDescriptions[iNormalAttributeOffset];
        normalAttribute.binding = iVertexBindingIndex;
        normalAttribute.location = iNormalAttributeOffset;
        normalAttribute.format = vec3Format;
        normalAttribute.offset = offsetof(MeshVertex, normal);

        // Describe UV attribute.
        auto& uvAttribute = vAttributeDescriptions[iUvAttributeOffset];
        uvAttribute.binding = iVertexBindingIndex;
        uvAttribute.location = iUvAttributeOffset;
        uvAttribute.format = vec2Format;
        uvAttribute.offset = offsetof(MeshVertex, uv);

        return vAttributeDescriptions;
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
        auto parseResult = ShaderIncluder::parseFullSourceCode(shaderDescription.pathToShaderFile);
        if (std::holds_alternative<ShaderIncluder::Error>(parseResult)) [[unlikely]] {
            return Error(fmt::format(
                "failed to parse shader source code, error: {}",
                convertShaderIncluderErrorToString(std::get<ShaderIncluder::Error>(parseResult))));
        }
        const auto sFullShaderSourceCode = std::get<std::string>(std::move(parseResult));

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
        std::vector<uint32_t> vCompiledBytecode(compilationResult.cbegin(), compilationResult.cend());

        // Make sure we can generate descriptor set layout without errors.
        auto layoutResult = DescriptorSetLayoutGenerator::collectInfoFromBytecode(
            vCompiledBytecode.data(), vCompiledBytecode.size() * sizeof(vCompiledBytecode[0]));
        if (std::holds_alternative<Error>(layoutResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(layoutResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        // Intentionally ignore results, we only care about errors here.

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
            vCompiledBytecode.size() * sizeof(vCompiledBytecode[0]));
        shaderCacheFile.close();

        return std::make_shared<GlslShader>(
            pRenderer,
            pathToCompiledShader,
            shaderDescription.sShaderName,
            shaderDescription.shaderType,
            shaderDescription.sShaderEntryFunctionName);
    }

    bool GlslShader::releaseShaderDataFromMemoryIfLoaded() {
        std::scoped_lock guard(mtxSpirvBytecode.first, mtxDescriptorSetLayoutBindingInfo.first);

        if (!mtxSpirvBytecode.second.empty()) {
            // Release bytecode.
            mtxSpirvBytecode.second.clear();
            mtxSpirvBytecode.second.shrink_to_fit();
        }

        if (mtxDescriptorSetLayoutBindingInfo.second.has_value()) {
            // Release descriptor set layout binding info.
            mtxDescriptorSetLayoutBindingInfo.second = {};
        }

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
    GlslShader::getCompiledBytecode() {
        // Load shader data from disk (if needed).
        auto optionalError = loadShaderDataFromDiskIfNotLoaded();
        if (optionalError.has_value()) [[unlikely]] {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return &mtxSpirvBytecode;
    }

    std::pair<std::mutex, std::optional<std::unordered_map<uint32_t, DescriptorSetLayoutBindingInfo>>>*
    GlslShader::getDescriptorSetLayoutBindingInfo() {
        return &mtxDescriptorSetLayoutBindingInfo;
    }

    std::string GlslShader::getShaderEntryFunctionName() const { return sShaderEntryFunctionName; }

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

    std::optional<Error> GlslShader::loadShaderDataFromDiskIfNotLoaded() {
        std::scoped_lock guard(mtxSpirvBytecode.first, mtxDescriptorSetLayoutBindingInfo.first);

        if (mtxSpirvBytecode.second.empty()) {
            // Get path to compiled shader.
            auto pathResult = getPathToCompiledShader();
            if (std::holds_alternative<Error>(pathResult)) {
                auto err = std::get<Error>(std::move(pathResult));
                err.addCurrentLocationToErrorStack();
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

        if (!mtxDescriptorSetLayoutBindingInfo.second.has_value()) {
            // Make sure bytecode was loaded.
            if (mtxSpirvBytecode.second.empty()) [[unlikely]] {
                return Error("expected shader bytecode to be loaded at this point");
            }

            // Generate descriptor set layout.
            auto layoutResult = DescriptorSetLayoutGenerator::collectInfoFromBytecode(
                mtxSpirvBytecode.second.data(),
                mtxSpirvBytecode.second.size() * sizeof(mtxSpirvBytecode.second[0]));
            if (std::holds_alternative<Error>(layoutResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(layoutResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            mtxDescriptorSetLayoutBindingInfo.second =
                std::get<std::unordered_map<uint32_t, DescriptorSetLayoutBindingInfo>>(
                    std::move(layoutResult));
        }

        return {};
    }

} // namespace ne
