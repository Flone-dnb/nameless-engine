#include "shaders/ShaderDescription.h"

// Custom.
#include "io/Logger.h"

// External.
#include "xxHash/xxhash.h"

namespace ne {
    ShaderDescription::ShaderDescription(
        const std::string& sShaderName,
        const std::filesystem::path& pathToShaderFile,
        ShaderType shaderType,
        const std::string& sShaderEntryFunctionName,
        const std::vector<std::string>& vDefinedShaderMacros) {
        this->sShaderName = sShaderName;
        this->pathToShaderFile = pathToShaderFile;
        this->shaderType = shaderType;
        this->sShaderEntryFunctionName = sShaderEntryFunctionName;
        this->vDefinedShaderMacros = vDefinedShaderMacros;
    }

    void ShaderDescription::from_toml(const toml::value& data) { // NOLINT
        vDefinedShaderMacros = toml::find_or<std::vector<std::string>>(
            data, "defined_shader_macros", std::vector<std::string>{});
        sShaderEntryFunctionName = toml::find_or<std::string>(data, "shader_entry_function_name", "");
        sSourceFileHash = toml::find_or<std::string>(data, "source_file_hash", "");
        shaderType = static_cast<ShaderType>(toml::find_or<int>(data, "shader_type", 0));
    }

    toml::value ShaderDescription::into_toml() const { // NOLINT
        toml::value outValue;
        outValue["defined_shader_macros"] = vDefinedShaderMacros;
        outValue["shader_entry_function_name"] = sShaderEntryFunctionName;
        outValue["shader_type"] = static_cast<int>(shaderType);

        if (sSourceFileHash.empty()) {
            outValue["source_file_hash"] = getShaderSourceFileHash(pathToShaderFile, sShaderName);
        } else {
            outValue["source_file_hash"] = sSourceFileHash;
        }

        return outValue;
    }

    std::string ShaderDescription::getShaderSourceFileHash(
        const std::filesystem::path& pathToShaderSourceFile, const std::string& sShaderName) {
        if (pathToShaderSourceFile.empty()) [[unlikely]] {
            Logger::get().error(std::format("path to shader file is empty (shader: {})", sShaderName), "");
            return "";
        }
        if (!std::filesystem::exists(pathToShaderSourceFile)) [[unlikely]] {
            Logger::get().error(
                std::format(
                    "shader file does not exist (shader: {}, path: {})",
                    sShaderName,
                    pathToShaderSourceFile.string()),
                "");
            return "";
        }

        std::ifstream sourceFile(pathToShaderSourceFile, std::ios::binary);

        sourceFile.seekg(0, std::ios::end);
        const long long iFileLength = sourceFile.tellg();
        sourceFile.seekg(0, std::ios::beg);

        std::vector<char> vFileData(iFileLength);

        sourceFile.read(vFileData.data(), iFileLength);
        sourceFile.close();

        return std::to_string(XXH3_64bits(vFileData.data(), iFileLength));
    }

    std::pair<bool, std::string> ShaderDescription::isSerializableDataEqual(const ShaderDescription& other) {
        if (sSourceFileHash.empty()) {
            sSourceFileHash = getShaderSourceFileHash(pathToShaderFile, sShaderName);
        }

        // Shader entry.
        if (sShaderEntryFunctionName != other.sShaderEntryFunctionName) {
            return std::make_pair(false, "shader entry function name changed");
        }

        // Shader type.
        if (shaderType != other.shaderType) {
            return std::make_pair(false, "shader type changed");
        }

        // Shader macro defines.
        if (vDefinedShaderMacros.size() != other.vDefinedShaderMacros.size()) {
            return std::make_pair(false, "defined shader macros changed");
        }
        if (!vDefinedShaderMacros.empty()) {
            for (const auto& macro : vDefinedShaderMacros) {
                auto it = std::ranges::find(other.vDefinedShaderMacros, macro);
                if (it == other.vDefinedShaderMacros.end()) {
                    return std::make_pair(false, "defined shader macros changed");
                }
            }
        }

        // Compare source file hashes.
        if (sSourceFileHash != other.sSourceFileHash) {
            return std::make_pair(false, "shader source file changed");
        }

        return std::make_pair(true, "");
    }
} // namespace ne
