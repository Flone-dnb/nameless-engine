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

        shaderIncludeTreeHashes = deserializeShaderIncludeTreeHashes(data);
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

        std::string sIncludeChainText(sInitialIncludeChainText);
        serializeShaderIncludeTree(pathToShaderFile, sIncludeChainText, outValue);

        return outValue;
    }

    std::string ShaderDescription::getShaderSourceFileHash(
        const std::filesystem::path& pathToShaderSourceFile, const std::string& sShaderName) {
        if (pathToShaderSourceFile.empty()) [[unlikely]] {
            Logger::get().error(
                std::format("path to shader file is empty (shader: {})", sShaderName),
                sShaderDescriptionLogCategory);
            return "";
        }
        if (!std::filesystem::exists(pathToShaderSourceFile)) [[unlikely]] {
            Logger::get().error(
                std::format(
                    "shader file does not exist (shader: {}, path: {})",
                    sShaderName,
                    pathToShaderSourceFile.string()),
                sShaderDescriptionLogCategory);
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

    void ShaderDescription::calculateShaderIncludeTreeHashes() {
        toml::value tomlTable;
        std::string sIncludeChainText(sInitialIncludeChainText);
        serializeShaderIncludeTree(pathToShaderFile, sIncludeChainText, tomlTable);

        if (tomlTable.is_uninitialized()) {
            // Shader source file has no "#include" entries.
            return;
        }

        shaderIncludeTreeHashes.clear();
        const auto& tableData = tomlTable.as_table();
        for (const auto& [sIncludeChain, table] : tableData) {
            shaderIncludeTreeHashes[sIncludeChain] =
                toml::get<std::unordered_map<std::string, std::string>>(table);
        }
    }

    std::unordered_map<
        std::string /** include chain (i.e. current shader) */,
        std::unordered_map<std::string /** relative include path */, std::string /** include file hash */>>
    ShaderDescription::deserializeShaderIncludeTreeHashes(const toml::value& data) {
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> includeTree;

        if (!data.is_table()) {
            Logger::get().error("data is not a table", sShaderDescriptionLogCategory);
            return includeTree;
        }

        const auto& dataTable = data.as_table();
        for (const auto& [sSectionName, sectionData] : dataTable) {
            if (sSectionName.starts_with(sInitialIncludeChainText)) {
                if (!sectionData.is_table()) {
                    Logger::get().error("section data is not a table", sShaderDescriptionLogCategory);
                    return includeTree;
                }
                const auto& sectionTable = sectionData.as_table();

                std::unordered_map<std::string, std::string> includes;
                for (const auto& [sInclude, sIncludeFileHash] : sectionTable) {
                    if (!sIncludeFileHash.is_string()) {
                        Logger::get().error(
                            "include file hash is not a string", sShaderDescriptionLogCategory);
                        return includeTree;
                    }
                    includes[sInclude] = sIncludeFileHash.as_string();
                }

                includeTree[sSectionName] = includes;
            }
        }

        return includeTree;
    }

    std::optional<ShaderCacheInvalidationReason>
    ShaderDescription::isSerializableDataEqual(ShaderDescription& other) {
        // Prepare source file hash.
        // Recalculate all here anyway because the file might change.
        if (!pathToShaderFile.empty()) { // data from cache will not have path to shader file
            sSourceFileHash = getShaderSourceFileHash(pathToShaderFile, sShaderName);
            calculateShaderIncludeTreeHashes();
        }
        if (!other.pathToShaderFile.empty()) { // data from cache will not have path to shader file
            other.sSourceFileHash = getShaderSourceFileHash(other.pathToShaderFile, other.sShaderName);
            other.calculateShaderIncludeTreeHashes();
        }

        // Shader entry.
        if (sShaderEntryFunctionName != other.sShaderEntryFunctionName) {
            return ShaderCacheInvalidationReason::ENTRY_FUNCTION_NAME_CHANGED;
        }

        // Shader type.
        if (shaderType != other.shaderType) {
            return ShaderCacheInvalidationReason::SHADER_TYPE_CHANGED;
        }

        // Shader macro defines.
        if (vDefinedShaderMacros.size() != other.vDefinedShaderMacros.size()) {
            return ShaderCacheInvalidationReason::DEFINED_SHADER_MACROS_CHANGED;
        }
        if (!vDefinedShaderMacros.empty()) {
            for (const auto& macro : vDefinedShaderMacros) {
                auto it = std::ranges::find(other.vDefinedShaderMacros, macro);
                if (it == other.vDefinedShaderMacros.end()) {
                    return ShaderCacheInvalidationReason::DEFINED_SHADER_MACROS_CHANGED;
                }
            }
        }

        // Compare source file hashes.
        if (sSourceFileHash != other.sSourceFileHash) {
            return ShaderCacheInvalidationReason::SHADER_SOURCE_FILE_CHANGED;
        }

        // Compare include tree.
        if (shaderIncludeTreeHashes != other.shaderIncludeTreeHashes) {
            return ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED;
        }

        return {};
    }

    void ShaderDescription::serializeShaderIncludeTree(
        const std::filesystem::path& pathToShaderFile, std::string& sCurrentIncludeChain, toml::value& data) {
        if (!std::filesystem::exists(pathToShaderFile)) {
            Logger::get().error(
                std::format("path to shader file \"{}\" does not exist", pathToShaderFile.string()),
                sShaderDescriptionLogCategory);
            return;
        }

        toml::value includesTable;

        std::ifstream shaderFile(pathToShaderFile, std::ios::binary);

        // Get file size.
        shaderFile.seekg(0, std::ios::end);
        long long iShaderFileSize = shaderFile.tellg();
        shaderFile.seekg(0, std::ios::beg);

        // Read file into memory.
        std::string sShaderFileText;
        sShaderFileText.resize(iShaderFileSize);
        shaderFile.read(sShaderFileText.data(), iShaderFileSize);
        shaderFile.close();

        // Find all "#include" entries.
        std::vector<std::string> vIncludePaths;
        constexpr std::string_view sKeyword = "#include";
        size_t iCurrentReadIndex = 0;

        do {
            auto iFoundPos = sShaderFileText.find(sKeyword, iCurrentReadIndex);
            if (iFoundPos == std::string::npos) {
                break;
            }

            iCurrentReadIndex = iFoundPos + sKeyword.size();

            if (iCurrentReadIndex >= sShaderFileText.size()) {
                break;
            }

            // Find opening '"' character.
            iFoundPos = sShaderFileText.find('\"', iCurrentReadIndex);
            if (iFoundPos == std::string::npos) {
                // See for '<' character then (don't know if you can actually use this character in shader
                // include but check anyway).
                iFoundPos = sShaderFileText.find('<', iCurrentReadIndex);
                if (iFoundPos == std::string::npos) {
                    Logger::get().error(
                        std::format(
                            "found \"{}\" but have not found \" or < character after it in the shader file "
                            "\"{}\"",
                            sKeyword,
                            pathToShaderFile.string()),
                        sShaderDescriptionLogCategory);
                    break;
                }
            }

            // Save include start index.
            iCurrentReadIndex = iFoundPos + 1;
            size_t iIncludeStartIndex = iCurrentReadIndex;

            if (iCurrentReadIndex >= sShaderFileText.size()) {
                break;
            }

            // Find closing '"' character.
            iFoundPos = sShaderFileText.find('\"', iCurrentReadIndex);
            if (iFoundPos == std::string::npos) {
                // See for '>' character then (don't know if you can actually use this character in shader
                // include but check anyway).
                iFoundPos = sShaderFileText.find('>', iCurrentReadIndex);
                if (iFoundPos == std::string::npos) {
                    Logger::get().error(
                        std::format(
                            "found \"{}\" but have not found \" or > character after it in the shader file "
                            "\"{}\"",
                            sKeyword,
                            pathToShaderFile.string()),
                        sShaderDescriptionLogCategory);
                    break;
                }
            }

            // Save include end index.
            size_t iIncludeEndIndex = iFoundPos;
            iCurrentReadIndex = iFoundPos + 1;

            // Add include path.
            vIncludePaths.push_back(
                sShaderFileText.substr(iIncludeStartIndex, iIncludeEndIndex - iIncludeStartIndex));
        } while (iCurrentReadIndex < sShaderFileText.size());

        if (vIncludePaths.empty()) {
            return;
        }

        // Don't need shader file anymore.
        sShaderFileText.clear();
        sShaderFileText.shrink_to_fit();

        std::vector<std::filesystem::path> vIncludePathsToScan;

        for (const auto& sInclude : vIncludePaths) {
            const auto pathToIncludeFile =
                std::filesystem::canonical(pathToShaderFile.parent_path()) / sInclude;
            if (!std::filesystem::exists(pathToIncludeFile)) {
                Logger::get().error(
                    std::format(
                        "shader ({}) include file ({}) does not exist",
                        pathToShaderFile.string(),
                        pathToIncludeFile.string()),
                    sShaderDescriptionLogCategory);
                continue;
            }

            vIncludePathsToScan.push_back(pathToIncludeFile);

            includesTable[sInclude] =
                getShaderSourceFileHash(pathToIncludeFile, pathToIncludeFile.stem().string());
        }

        // Don't need include strings anymore.
        vIncludePaths.clear();
        vIncludePaths.shrink_to_fit();

        if (vIncludePathsToScan.empty()) {
            return;
        }

        const auto sFilename = pathToShaderFile.stem().string();
        sCurrentIncludeChain = std::format("{}.{}", sCurrentIncludeChain, sFilename);
        data[sCurrentIncludeChain] = includesTable;

        // Recursively do the same for all includes.
        for (const auto& includePath : vIncludePathsToScan) {
            serializeShaderIncludeTree(includePath, sCurrentIncludeChain, data);
        }
    }
} // namespace ne