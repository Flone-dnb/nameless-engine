#include "GlslShader.h"

// Standard.
#include <fstream>
#include <climits>
#include <filesystem>
#include <format>

// Custom.
#include "io/Logger.h"
#include "misc/Globals.h"
#include "shader/general/ShaderFilesystemPaths.hpp"
#include "game/nodes/MeshNode.h"
#include "misc/Profiler.hpp"
#include "render/vulkan/VulkanRenderer.h"

// External.
#include "CombinedShaderLanguageParser.h"

namespace ne {
    std::string getLineFromText(const std::string& sText, size_t iLineNumber) {
        // Prepare some variables.
        size_t iCurrentLineNumber = 1;
        size_t iLineStartPosition = 0;

        // Find requested line.
        do {
            iLineStartPosition = sText.find('\n', iLineStartPosition);
            if (iLineStartPosition == std::string::npos) {
                return "failed to get line text";
            }
            iLineStartPosition += 1;

            iCurrentLineNumber += 1;
        } while (iCurrentLineNumber != iLineNumber);

        // Make sure we have some text to copy.
        if (iLineStartPosition >= sText.size()) {
            return "failed to get line text";
        }

        // Find end of this line.
        const auto iLineEndPosition = sText.find('\n', iLineStartPosition);
        if (iLineEndPosition == std::string::npos) {
            return "failed to get line text";
        }

        return sText.substr(iLineStartPosition, iLineEndPosition - iLineStartPosition);
    }

    GlslShader::GlslShader(
        Renderer* pRenderer,
        std::filesystem::path pathToCompiledShader,
        const std::string& sShaderName,
        ShaderType shaderType)
        : Shader(pRenderer, std::move(pathToCompiledShader), sShaderName, shaderType) {
        // Make sure vertex attributes don't need update.
        static_assert(
            sizeof(MeshVertex) == 32, // NOLINT: current size
            "`getVertexAttributeDescriptions` needs to be updated");
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
        auto parseResult = CombinedShaderLanguageParser::parseGlsl(shaderDescription.pathToShaderFile);
        if (std::holds_alternative<CombinedShaderLanguageParser::Error>(parseResult)) [[unlikely]] {
            auto error = std::get<CombinedShaderLanguageParser::Error>(parseResult);
            return Error(std::format(
                "failed to parse shader source code, error: {} (while processing file: {})",
                error.sErrorMessage,
                error.pathToErrorFile.string()));
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
        compileOptions.SetOptimizationLevel(shaderc_optimization_level_zero);
#else
        compileOptions.SetOptimizationLevel(shaderc_optimization_level_performance);
#endif

        // Generate debug info in any build mode for valid reflection (otherwise binding names will be
        // not available).
        compileOptions.SetGenerateDebugInfo();

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
            auto sCompilationErrorMessage = compilationResult.GetErrorMessage();
            // Since compilation error usually points to a line number but we combine all included files
            // into one it may be hard to read error messages, thus if a message has a line number specified
            // append this line to the error message.
            // Find line where it says file name and error line, example: `MeshNode:98: error:`.
            const auto sFileLineText = sShaderSourceFileName + ":";
            const auto iFileLinePos = sCompilationErrorMessage.find(sFileLineText);
            if (iFileLinePos == std::string::npos) {
                return sCompilationErrorMessage;
            }
            // Find the second semicolon.
            const auto iLineTextStartPos = iFileLinePos + sFileLineText.size();
            const auto iSecondSemicolonPos = sCompilationErrorMessage.find(':', iLineTextStartPos);
            if (iSecondSemicolonPos == std::string::npos) {
                return sCompilationErrorMessage;
            }
            const auto sLineText =
                sCompilationErrorMessage.substr(iLineTextStartPos, iSecondSemicolonPos - iLineTextStartPos);
            try {
                const auto iLineNumber = std::stoull(sLineText);
                sCompilationErrorMessage += std::format(
                    "\nline {}: {}", iLineNumber, getLineFromText(sFullShaderSourceCode, iLineNumber));
            } catch (...) {
                return sCompilationErrorMessage;
            }

            return sCompilationErrorMessage;
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
            return Error(std::format(
                "failed to open the path \"{}\" for writing to save shader bytecode",
                pathToCompiledShader.string()));
        }
        shaderCacheFile.write(
            reinterpret_cast<const char*>(vCompiledBytecode.data()),
            vCompiledBytecode.size() * sizeof(vCompiledBytecode[0]));
        shaderCacheFile.close();

        return std::make_shared<GlslShader>(
            pRenderer, pathToCompiledShader, shaderDescription.sShaderName, shaderDescription.shaderType);
    }

    bool GlslShader::releaseShaderDataFromMemoryIfLoaded() {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxSpirvBytecode.first, mtxDescriptorSetLayoutInfo.first);

        if (!mtxSpirvBytecode.second.empty()) {
            // Release bytecode.
            mtxSpirvBytecode.second.clear();
            mtxSpirvBytecode.second.shrink_to_fit();

            notifyShaderBytecodeReleasedFromMemory();
        }

        if (mtxDescriptorSetLayoutInfo.second.has_value()) {
            // Release descriptor set layout info.
            mtxDescriptorSetLayoutInfo.second = {};
        }

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

    std::pair<std::mutex, std::optional<DescriptorSetLayoutGenerator::Collected>>*
    GlslShader::getDescriptorSetLayoutInfo() {
        return &mtxDescriptorSetLayoutInfo;
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

    std::optional<Error> GlslShader::loadShaderDataFromDiskIfNotLoaded() {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxSpirvBytecode.first, mtxDescriptorSetLayoutInfo.first);

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
            std::ifstream file(pathToCompiledShader, std::ifstream::ate | std::ios::binary);
            if (!file.is_open()) [[unlikely]] {
                return Error(std::format("failed to open the file \"{}\"", pathToCompiledShader.string()));
            }

            // Get file size.
            const long long iFileByteCount = file.tellg();
            file.seekg(0, std::ios::beg);

            // Prepare data.
            mtxSpirvBytecode.second.resize(iFileByteCount);

            // Read file contents.
            file.read(mtxSpirvBytecode.second.data(), mtxSpirvBytecode.second.size());

            // Close file.
            file.close();

            notifyShaderBytecodeLoadedIntoMemory();
        }

        if (!mtxDescriptorSetLayoutInfo.second.has_value()) {
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
            mtxDescriptorSetLayoutInfo.second =
                std::get<DescriptorSetLayoutGenerator::Collected>(std::move(layoutResult));
        }

        return {};
    }

} // namespace ne
