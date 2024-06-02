#include "shader/ShaderDescription.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"

// External.
#include "xxHash/xxhash.h"

namespace ne {
    ShaderDescription::ShaderDescription(
        const std::string& sShaderName,
        const std::filesystem::path& pathToShaderFile,
        ShaderType shaderType,
        std::optional<VertexFormat> vertexFormat,
        const std::string& sShaderEntryFunctionName,
        const std::unordered_map<std::string, std::string>& definedShaderMacros) {
        this->sShaderName = sShaderName;
        this->pathToShaderFile = pathToShaderFile;
        this->shaderType = shaderType;
        this->sShaderEntryFunctionName = sShaderEntryFunctionName;
        this->definedShaderMacros = definedShaderMacros;
        this->vertexFormat = vertexFormat;
    }

    void ShaderDescription::from_toml(const toml::value& data) { // NOLINT
        definedShaderMacros = toml::find_or<std::unordered_map<std::string, std::string>>(
            data, "defined_shader_macros", std::unordered_map<std::string, std::string>{});

        sShaderEntryFunctionName = toml::find_or<std::string>(data, "shader_entry_function_name", "");

        sSourceFileHash = toml::find_or<std::string>(data, "source_file_hash", "");

        shaderType = static_cast<ShaderType>(toml::find_or<int>(data, "shader_type", 0));

        shaderIncludeTreeHashes = deserializeShaderIncludeTreeHashes(data);
    }

    toml::value ShaderDescription::into_toml() const { // NOLINT
        toml::value outValue;
        outValue["defined_shader_macros"] = definedShaderMacros;
        outValue["shader_entry_function_name"] = sShaderEntryFunctionName;
        outValue["shader_type"] = static_cast<int>(shaderType);

        if (sSourceFileHash.empty()) {
            outValue["source_file_hash"] = getFileHash(pathToShaderFile, sShaderName);
        } else {
            outValue["source_file_hash"] = sSourceFileHash;
        }

        std::string sIncludeChainText(sInitialIncludeChainText);
        serializeShaderIncludeTree(pathToShaderFile, sIncludeChainText, outValue);

        return outValue;
    }

    std::string
    ShaderDescription::getFileHash(const std::filesystem::path& pathToFile, const std::string& sShaderName) {
        if (pathToFile.empty()) [[unlikely]] {
            Logger::get().error(std::format("path to file is empty (shader: {})", sShaderName));
            return "";
        }
        if (!std::filesystem::exists(pathToFile)) [[unlikely]] {
            Logger::get().error(
                std::format("file does not exist (shader: {}, path: {})", sShaderName, pathToFile.string()));
            return "";
        }

        std::ifstream sourceFile(pathToFile, std::ios::binary);

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
            Logger::get().error("data is not a table");
            return includeTree;
        }

        const auto& dataTable = data.as_table();
        for (const auto& [sSectionName, sectionData] : dataTable) {
            if (sSectionName.starts_with(sInitialIncludeChainText)) {
                if (!sectionData.is_table()) {
                    Logger::get().error("section data is not a table");
                    return includeTree;
                }
                const auto& sectionTable = sectionData.as_table();

                std::unordered_map<std::string, std::string> includes;
                for (const auto& [sInclude, sIncludeFileHash] : sectionTable) {
                    if (!sIncludeFileHash.is_string()) {
                        Logger::get().error("include file hash is not a string");
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
        // Calculate source file hashes if path to shader file is specified to compare them later.
        // If a shader description was retrieved from cache it will not have path to shader file specified.
        // (recalculate hashes here because the file might be changed at this point)
        if (!pathToShaderFile.empty()) { // data from cache will not have path to shader file
            sSourceFileHash = getFileHash(pathToShaderFile, sShaderName);
            calculateShaderIncludeTreeHashes();
        }
        if (!other.pathToShaderFile.empty()) { //
            other.sSourceFileHash = getFileHash(other.pathToShaderFile, other.sShaderName);
            other.calculateShaderIncludeTreeHashes();
        }

        // Make sure source file hashes are filled.
        if (sSourceFileHash.empty() && other.sSourceFileHash.empty()) [[unlikely]] {
            Logger::get().error(std::format(
                "unable to compare the specified shader descriptions \"{}\" and \"{}\" because their "
                "shader source file hashes are empty and it's impossible to calculate them because "
                "path to the shader source file also seems to be empty",
                sShaderName,
                other.sShaderName));
            return ShaderCacheInvalidationReason::SHADER_SOURCE_FILE_CHANGED;
        }

        // Compare shader entry function name.
        if (sShaderEntryFunctionName != other.sShaderEntryFunctionName) {
            return ShaderCacheInvalidationReason::ENTRY_FUNCTION_NAME_CHANGED;
        }

        // Compare shader type.
        if (shaderType != other.shaderType) {
            return ShaderCacheInvalidationReason::SHADER_TYPE_CHANGED;
        }

        // Compare shader macro defines.
        if (definedShaderMacros.size() != other.definedShaderMacros.size()) {
            return ShaderCacheInvalidationReason::DEFINED_SHADER_MACROS_CHANGED;
        }
        for (const auto& [sMacro, sValue] : definedShaderMacros) {
            const auto it = other.definedShaderMacros.find(sMacro);
            if (it == other.definedShaderMacros.end()) {
                return ShaderCacheInvalidationReason::DEFINED_SHADER_MACROS_CHANGED;
            }

            if (sValue != it->second) {
                return ShaderCacheInvalidationReason::DEFINED_SHADER_MACROS_CHANGED;
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
                std::format("path to shader file \"{}\" does not exist", pathToShaderFile.string()));
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
                    Logger::get().error(std::format(
                        "found \"{}\" but have not found \" or < character after it in the shader file "
                        "\"{}\"",
                        sKeyword,
                        pathToShaderFile.string()));
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
                    Logger::get().error(std::format(
                        "found \"{}\" but have not found \" or > character after it in the shader file "
                        "\"{}\"",
                        sKeyword,
                        pathToShaderFile.string()));
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
                Logger::get().error(std::format(
                    "shader ({}) include file ({}) does not exist",
                    pathToShaderFile.string(),
                    pathToIncludeFile.string()));
                continue;
            }

            vIncludePathsToScan.push_back(pathToIncludeFile);

            includesTable[sInclude] = getFileHash(pathToIncludeFile, pathToIncludeFile.stem().string());
        }

        // Don't need include strings anymore.
        vIncludePaths.clear();
        vIncludePaths.shrink_to_fit();

        // Exit if there are no more include paths to scan.
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
