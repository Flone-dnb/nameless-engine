#include "HlslShader.h"

// Standard.
#include <fstream>
#include <climits>
#include <filesystem>
#include <format>

// Custom.
#include "io/Logger.h"
#include "misc/Globals.h"
#include "render/directx/DirectXRenderer.h"
#include "shader/general/ShaderFilesystemPaths.hpp"
#include "misc/Profiler.hpp"

// External.
#include "CombinedShaderLanguageParser.h"
#include "DirectXShaderCompiler/inc/d3d12shader.h"
#pragma comment(lib, "dxcompiler.lib")

namespace ne {
    HlslShader::HlslShader(
        Renderer* pRenderer,
        std::filesystem::path pathToCompiledShader,
        const std::string& sShaderName,
        ShaderType shaderType,
        const std::optional<VertexFormat>& vertexFormat,
        const std::string& sSourceFileHash)
        : Shader(pRenderer, std::move(pathToCompiledShader), sShaderName, shaderType, vertexFormat) {
        // Save source file hash.
        this->sSourceFileHash = sSourceFileHash;
    }

    std::variant<std::string, Error> HlslShader::getShaderCompilerVersion() {
        // Get DXC compiler.
        ComPtr<IDxcCompiler3> pCompiler;
        HRESULT hResult = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        // Get version info.
        ComPtr<IDxcVersionInfo2> pCompilerVersionInfo;
        hResult = pCompiler.As(&pCompilerVersionInfo);
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }
        UINT32 iCommitCount = 0;
        char* pCommitSha = nullptr;
        hResult = pCompilerVersionInfo->GetCommitInfo(&iCommitCount, &pCommitSha);
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        return std::string(pCommitSha);
    }

    CD3DX12_STATIC_SAMPLER_DESC
    HlslShader::getStaticSamplerDescription(TextureFilteringQuality textureFilteringQuality) {
        switch (textureFilteringQuality) {
        case (TextureFilteringQuality::LOW): {
            return CD3DX12_STATIC_SAMPLER_DESC(
                static_cast<UINT>(StaticSamplerShaderRegister::BASIC),
                D3D12_FILTER_MIN_MAG_MIP_POINT,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                0.0F,
                16, // NOLINT: magic number, max anisotropy
                D3D12_COMPARISON_FUNC_LESS_EQUAL,
                D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
                0.0F,
                D3D12_FLOAT32_MAX,
                D3D12_SHADER_VISIBILITY_ALL,
                iStaticSamplerShaderRegisterSpace);
            break;
        }
        case (TextureFilteringQuality::MEDIUM): {
            return CD3DX12_STATIC_SAMPLER_DESC(
                static_cast<UINT>(StaticSamplerShaderRegister::BASIC),
                D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                0.0F,
                16, // NOLINT: magic number, max anisotropy
                D3D12_COMPARISON_FUNC_LESS_EQUAL,
                D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
                0.0F,
                D3D12_FLOAT32_MAX,
                D3D12_SHADER_VISIBILITY_ALL,
                iStaticSamplerShaderRegisterSpace);
            break;
        }
        case (TextureFilteringQuality::HIGH): {
            return CD3DX12_STATIC_SAMPLER_DESC(
                static_cast<UINT>(StaticSamplerShaderRegister::BASIC),
                D3D12_FILTER_ANISOTROPIC,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                0.0F,
                16, // NOLINT: magic number, max anisotropy
                D3D12_COMPARISON_FUNC_LESS_EQUAL,
                D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
                0.0F,
                D3D12_FLOAT32_MAX,
                D3D12_SHADER_VISIBILITY_ALL,
                iStaticSamplerShaderRegisterSpace);
            break;
        }
        }

        Error error("unhandled case");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    CD3DX12_STATIC_SAMPLER_DESC HlslShader::getStaticComparisonSamplerDescription() {
        return CD3DX12_STATIC_SAMPLER_DESC(
            static_cast<UINT>(StaticSamplerShaderRegister::COMPARISON),
            D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            0.0F,
            16,                               // NOLINT: magic number, max anisotropy
            D3D12_COMPARISON_FUNC_LESS_EQUAL, // same as in Vulkan
            D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
            0.0F,
            D3D12_FLOAT32_MAX,
            D3D12_SHADER_VISIBILITY_ALL,
            iStaticSamplerShaderRegisterSpace);
    }

    UINT HlslShader::getStaticSamplerShaderRegisterSpace() { return iStaticSamplerShaderRegisterSpace; }

    std::variant<ComPtr<IDxcResult>, std::string, Error> HlslShader::compileShaderToBytecode(
        const ShaderDescription& shaderDescription, const std::optional<std::filesystem::path>& pathToPdb) {
        // Create utils.
        ComPtr<IDxcUtils> pUtils;
        HRESULT hResult = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        // Create compiler.
        ComPtr<IDxcCompiler3> pCompiler;
        hResult = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        // Create default include handler.
        ComPtr<IDxcIncludeHandler> pIncludeHandler;
        hResult = pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Create shader model string.
        std::wstring sShaderModel;
        switch (shaderDescription.shaderType) {
        case ShaderType::VERTEX_SHADER:
            sShaderModel = Globals::stringToWstring(std::string(sVertexShaderModel));
            break;
        case ShaderType::FRAGMENT_SHADER:
            sShaderModel = Globals::stringToWstring(std::string(sPixelShaderModel));
            break;
        case ShaderType::COMPUTE_SHADER:
            sShaderModel = Globals::stringToWstring(std::string(sComputeShaderModel));
            break;
        }

        // Convert std::string to std::wstring to be used.
        std::wstring sShaderEntry(
            shaderDescription.sShaderEntryFunctionName.begin(),
            shaderDescription.sShaderEntryFunctionName.end());

        // Prepare compilation arguments.
        std::vector<std::wstring> vArgs;
        vArgs.push_back(shaderDescription.pathToShaderFile);
        vArgs.push_back(L"-E");
        vArgs.push_back(sShaderEntry);
        vArgs.push_back(L"-T");
        vArgs.push_back(sShaderModel);
        vArgs.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
#if defined(DEBUG)
        vArgs.push_back(DXC_ARG_DEBUG);
        vArgs.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
        if (pathToPdb.has_value()) {
            vArgs.push_back(L"-Fd");
            vArgs.push_back(pathToPdb.value());
        }
#else
        vArgs.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
#endif

        // Add macros.
        for (const auto& [sMacro, sValue] : shaderDescription.definedShaderMacros) {
            vArgs.push_back(L"-D");
            if (sValue.empty()) {
                vArgs.push_back(Globals::stringToWstring(sMacro));
            } else {
                vArgs.push_back(Globals::stringToWstring(std::format("{}={}", sMacro, sValue)));
            }
        }

        // Make sure the file has extension.
        if (!shaderDescription.pathToShaderFile.has_extension()) [[unlikely]] {
            return Error(std::format(
                "expected the file \"{}\" to have an extension",
                shaderDescription.pathToShaderFile.string()));
        }

        // Get shader file extension.
        auto shaderFileExtension = shaderDescription.pathToShaderFile.extension().string();

        // Transform it to lowercase.
        std::transform(
            shaderFileExtension.begin(),
            shaderFileExtension.end(),
            shaderFileExtension.begin(),
            [](unsigned char c) { return std::tolower(c); });

        std::string sFullShaderSourceCode;

        // Parse source code.
        auto parseResult = CombinedShaderLanguageParser::parseHlsl(shaderDescription.pathToShaderFile);
        if (std::holds_alternative<CombinedShaderLanguageParser::Error>(parseResult)) [[unlikely]] {
            auto error = std::get<CombinedShaderLanguageParser::Error>(parseResult);
            return Error(std::format(
                "failed to parse shader source code, error: {} (while processing file: {})",
                error.sErrorMessage,
                error.pathToErrorFile.string()));
        }
        sFullShaderSourceCode = std::get<std::string>(std::move(parseResult));

        // Load source code.
        ComPtr<IDxcBlobEncoding> pSource = nullptr;
        hResult = pUtils->CreateBlob(
            sFullShaderSourceCode.data(),
            static_cast<UINT32>(sFullShaderSourceCode.size()),
            iShaderFileCodepage,
            &pSource);
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        DxcBuffer sourceShaderBuffer{};
        sourceShaderBuffer.Ptr = pSource->GetBufferPointer();
        sourceShaderBuffer.Size = pSource->GetBufferSize();
        sourceShaderBuffer.Encoding = iShaderFileCodepage;

        // Convert arguments.
        std::vector<LPCWSTR> vFixedArguments(vArgs.size());
        for (size_t i = 0; i < vArgs.size(); i++) {
            vFixedArguments[i] = vArgs[i].c_str();
        }

        // Compile it with specified arguments.
        ComPtr<IDxcResult> pResults;
        hResult = pCompiler->Compile(
            &sourceShaderBuffer,
            vFixedArguments.data(),
            static_cast<UINT32>(vArgs.size()),
            pIncludeHandler.Get(),
            IID_PPV_ARGS(&pResults));
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        // See if errors occurred.
        ComPtr<IDxcBlobUtf8> pErrors = nullptr;
        hResult = pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }
        if (pErrors != nullptr && pErrors->GetStringLength() > 0) [[unlikely]] {
            return std::string{pErrors->GetStringPointer(), pErrors->GetStringLength()};
        }

        // See if the compilation failed.
        pResults->GetStatus(&hResult);
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        return pResults;
    }

    std::variant<std::shared_ptr<Shader>, std::string, Error> HlslShader::compileShader(
        Renderer* pRenderer,
        const std::filesystem::path& cacheDirectory,
        const std::string& sConfiguration,
        const ShaderDescription& shaderDescription) {
        // Check that the renderer is DirectX renderer.
        const auto pDirectXRenderer = dynamic_cast<DirectXRenderer*>(pRenderer);
        if (pDirectXRenderer == nullptr) [[unlikely]] {
            return Error("the specified renderer is not a DirectX renderer");
        }

        // Calculate source file hash (to use later) but make sure it's not empty.
        const auto sSourceFileHash =
            ShaderDescription::getFileHash(shaderDescription.pathToShaderFile, shaderDescription.sShaderName);
        if (sSourceFileHash.empty()) [[unlikely]] {
            return Error(std::format(
                "unable to calculate shader source file hash (shader path: \"{}\")",
                shaderDescription.pathToShaderFile.string()));
        }

        // Prepare path to the PDB file.
        auto pathToPdb = cacheDirectory / ShaderFilesystemPaths::getShaderCacheBaseFileName();
        pathToPdb += sConfiguration;
        pathToPdb += ".pdb";

        // Compile.
        auto compilationResult = compileShaderToBytecode(shaderDescription, pathToPdb);
        if (std::holds_alternative<Error>(compilationResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(compilationResult));
            error.addCurrentLocationToErrorStack();
            return error;
        } else if (std::holds_alternative<std::string>(std::move(compilationResult))) [[unlikely]] {
            return std::get<std::string>(std::move(compilationResult));
        }
        auto pResults = std::get<ComPtr<IDxcResult>>(std::move(compilationResult));

        // Get reflection.
        ComPtr<IDxcBlob> pReflectionData;
        HRESULT hResult = pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr);
        if (FAILED(hResult)) {
            return Error(hResult);
        }
        if (pReflectionData == nullptr) {
            return Error("failed to get reflection data");
        }

        // Get compiled shader binary.
        ComPtr<IDxcBlob> pCompiledShaderBlob = nullptr;
        ComPtr<IDxcBlobUtf16> pShaderName = nullptr;
        pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pCompiledShaderBlob), &pShaderName);
        if (pCompiledShaderBlob == nullptr) {
            return Error(std::format(
                "no shader binary was generated for {}", shaderDescription.pathToShaderFile.string()));
        }

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
            static_cast<char*>(pCompiledShaderBlob->GetBufferPointer()),
            static_cast<long long>(pCompiledShaderBlob->GetBufferSize()));
        shaderCacheFile.close();

        // Create utils.
        ComPtr<IDxcUtils> pUtils;
        hResult = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
        if (FAILED(hResult)) [[unlikely]] {
            return Error(hResult);
        }

        // Create reflection interface.
        DxcBuffer reflectionData{};
        reflectionData.Encoding = iShaderFileCodepage;
        reflectionData.Ptr = pReflectionData->GetBufferPointer();
        reflectionData.Size = pReflectionData->GetBufferSize();
        ComPtr<ID3D12ShaderReflection> pReflection;
        hResult = pUtils->CreateReflection(&reflectionData, IID_PPV_ARGS(&pReflection));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Write reflection data to file.
        const auto pathToShaderReflection =
            (cacheDirectory / ShaderFilesystemPaths::getShaderCacheBaseFileName()).string() + sConfiguration +
            sShaderReflectionFileExtension;
        std::ofstream shaderReflectionFile(pathToShaderReflection, std::ios::binary);
        if (!shaderReflectionFile.is_open()) {
            return Error(std::format("failed to save shader reflection data at {}", pathToShaderReflection));
        }
        shaderReflectionFile.write(
            static_cast<char*>(pReflectionData->GetBufferPointer()),
            static_cast<long long>(pReflectionData->GetBufferSize()));
        shaderReflectionFile.close();

#if defined(DEBUG)
        // Save PDB file.
        ComPtr<IDxcBlob> pShaderPdb = nullptr;
        ComPtr<IDxcBlobUtf16> pShaderPdbName = nullptr;
        pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pShaderPdb), &pShaderPdbName);
        if (pShaderPdb == nullptr) [[unlikely]] {
            return Error(
                std::format("no PDB was generated for {}", shaderDescription.pathToShaderFile.string()));
        }

        std::ofstream shaderPdbFile(pathToPdb, std::ios::binary);
        if (!shaderPdbFile.is_open()) [[unlikely]] {
            return Error(std::format("failed to save shader PDB at {}", pathToPdb.string()));
        }
        shaderPdbFile.write(
            static_cast<char*>(pShaderPdb->GetBufferPointer()),
            static_cast<long long>(pShaderPdb->GetBufferSize()));
        shaderPdbFile.close();
#endif

        // Create shader instance.
        auto pShader = std::make_shared<HlslShader>(
            pRenderer,
            pathToCompiledShader,
            shaderDescription.sShaderName,
            shaderDescription.shaderType,
            shaderDescription.vertexFormat,
            sSourceFileHash);

        // Make sure we are able to collect root signature info from reflection
        // (check that there are no errors).
        auto result =
            RootSignatureGenerator::collectInfoFromReflection(pDirectXRenderer->getD3dDevice(), pReflection);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto err = std::get<Error>(std::move(result));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        // Ignore results (will be later used from cache), right now we just checked for errors.

        return pShader;
    }

    std::variant<ComPtr<IDxcBlob>, Error> HlslShader::getCompiledBlob() {
        // Load shader data from disk (if needed).
        auto optionalError = loadShaderDataFromDiskIfNotLoaded();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return mtxCompiledBytecode.second;
    }

    std::pair<std::mutex, std::optional<RootSignatureGenerator::CollectedInfo>>*
    HlslShader::getRootSignatureInfo() {
        return &mtxRootSignatureInfo;
    }

    std::string HlslShader::getShaderSourceFileHash() const { return sSourceFileHash; }

    bool HlslShader::releaseShaderDataFromMemoryIfLoaded() {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxCompiledBytecode.first, mtxRootSignatureInfo.first);

        // Release shader bytecode.
        if (mtxCompiledBytecode.second != nullptr) {
            const auto iNewRefCount = mtxCompiledBytecode.second.Reset();
            if (iNewRefCount != 0) [[unlikely]] {
                Logger::get().error(std::format(
                    "shader \"{}\" bytecode was requested to be released from the "
                    "memory but it's still being referenced (new ref count: {})",
                    getShaderName(),
                    iNewRefCount));
            }

            notifyShaderBytecodeReleasedFromMemory();
        }

        // Release root signature info.
        if (mtxRootSignatureInfo.second.has_value()) {
            mtxRootSignatureInfo.second = {};
        }

        return false;
    }

    std::optional<Error>
    HlslShader::saveAdditionalCompilationResultsInfo(ConfigManager& cacheMetadataConfigManager) {
        // Calculate hash of reflection file.
        auto result = calculateReflectionFileHash();
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto sReflectionFileHash = std::get<std::string>(std::move(result));

        // Save hash of the reflection file to later test during cache validation.
        cacheMetadataConfigManager.setValue<std::string>(
            sHlslSectionName, sReflectionFileHashKeyName, sReflectionFileHash);

        return {};
    }

    std::optional<Error> HlslShader::checkCachedAdditionalCompilationResultsInfo(
        ConfigManager& cacheMetadataConfigManager,
        std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) {
        // Calculate hash of reflection file.
        auto result = calculateReflectionFileHash();
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto sReflectionFileHash = std::get<std::string>(std::move(result));

        // Read cached hash of the reflection file.
        const auto sCachedReflectionFileHash = cacheMetadataConfigManager.getValue<std::string>(
            sHlslSectionName, sReflectionFileHashKeyName, "");

        // Compare reflection file hashes.
        if (sCachedReflectionFileHash != sReflectionFileHash) [[unlikely]] {
            cacheInvalidationReason = ShaderCacheInvalidationReason::COMPILED_BINARY_CHANGED;
            return {};
        }

        return {};
    }

    std::variant<ComPtr<IDxcBlob>, Error>
    HlslShader::readBlobFromDisk(const std::filesystem::path& pathToFile) {
        // Open file.
        std::ifstream shaderBytecodeFile(pathToFile, std::ios::binary);
        if (!shaderBytecodeFile.is_open()) {
            return Error(std::format("failed to open file at {}", pathToFile.string()));
        }

        // Get file size.
        shaderBytecodeFile.seekg(0, std::ios::end);
        const long long iFileByteCount = shaderBytecodeFile.tellg();
        shaderBytecodeFile.seekg(0, std::ios::beg);

        if (iFileByteCount > static_cast<long long>(UINT_MAX)) {
            shaderBytecodeFile.close();
            return Error("blob file is too big");
        }

        const auto iBlobSize = static_cast<unsigned int>(iFileByteCount);

        // Read file to memory.
        const auto pBlobData = std::make_unique<char[]>(iBlobSize);
        shaderBytecodeFile.read(pBlobData.get(), iBlobSize);
        shaderBytecodeFile.close();

        ComPtr<IDxcUtils> pUtils;
        HRESULT hResult = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        ComPtr<IDxcBlob> pBlob;
        hResult = pUtils->CreateBlob(
            pBlobData.get(),
            static_cast<unsigned int>(iBlobSize),
            iShaderFileCodepage,
            reinterpret_cast<IDxcBlobEncoding**>(pBlob.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return pBlob;
    }

    std::variant<std::string, Error> HlslShader::calculateReflectionFileHash() {
        // Get path to compiled shader.
        auto pathToCompiledShaderResult = getPathToCompiledShader();
        if (std::holds_alternative<Error>(pathToCompiledShaderResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(pathToCompiledShaderResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto pathToCompiledShader =
            std::get<std::filesystem::path>(std::move(pathToCompiledShaderResult));

        // Make sure there is no extension (expecting the file to not have extension).
        if (pathToCompiledShader.has_extension()) [[unlikely]] {
            return Error(std::format(
                "expected the shader bytecode file \"{}\" to not have an extension",
                pathToCompiledShader.string()));
        }

        // Add extension that reflection binary files use.
        const auto pathToReflectionFile =
            std::filesystem::path(pathToCompiledShader.string() + sShaderReflectionFileExtension);

        // Make sure the reflection file exists.
        if (!std::filesystem::exists(pathToReflectionFile)) [[unlikely]] {
            return Error(
                std::format("expected reflection file to exist at \"{}\"", pathToReflectionFile.string()));
        }

        // Calculate hash of the reflection file.
        auto sReflectionFileHash = ShaderDescription::getFileHash(pathToReflectionFile, getShaderName());
        if (sReflectionFileHash.empty()) [[unlikely]] {
            return Error(
                std::format("failed to calculate hash of the file at \"{}\"", pathToReflectionFile.string()));
        }

        return sReflectionFileHash;
    }

    std::optional<Error> HlslShader::loadShaderDataFromDiskIfNotLoaded() {
        PROFILE_FUNC;

        std::scoped_lock guard(mtxCompiledBytecode.first, mtxRootSignatureInfo.first);

        // Get path to compiled shader.
        auto pathResult = getPathToCompiledShader();
        if (std::holds_alternative<Error>(pathResult)) {
            auto err = std::get<Error>(std::move(pathResult));
            err.addCurrentLocationToErrorStack();
            return err;
        }
        const auto pathToCompiledShader = std::get<std::filesystem::path>(std::move(pathResult));

        if (mtxCompiledBytecode.second == nullptr) {
            // Load cached bytecode from disk.
            auto result = readBlobFromDisk(pathToCompiledShader);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                return err;
            }

            mtxCompiledBytecode.second = std::get<ComPtr<IDxcBlob>>(std::move(result));

            notifyShaderBytecodeLoadedIntoMemory();
        }

        if (!mtxRootSignatureInfo.second.has_value()) {
            // Load shader reflection from disk.
            const auto pathToShaderReflection =
                pathToCompiledShader.string() + sShaderReflectionFileExtension;

            auto blobResult = readBlobFromDisk(pathToShaderReflection);
            if (std::holds_alternative<Error>(blobResult)) {
                auto err = std::get<Error>(std::move(blobResult));
                err.addCurrentLocationToErrorStack();
                return err;
            }
            const auto pReflectionData = std::get<ComPtr<IDxcBlob>>(std::move(blobResult));

            // Create utils.
            ComPtr<IDxcUtils> pUtils;
            HRESULT hResult = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
            if (FAILED(hResult)) {
                return Error(hResult);
            }

            // Create reflection interface.
            DxcBuffer reflectionData{};
            reflectionData.Encoding = iShaderFileCodepage;
            reflectionData.Ptr = pReflectionData->GetBufferPointer();
            reflectionData.Size = pReflectionData->GetBufferSize();
            ComPtr<ID3D12ShaderReflection> pReflection;
            hResult = pUtils->CreateReflection(&reflectionData, IID_PPV_ARGS(&pReflection));
            if (FAILED(hResult)) {
                return Error(hResult);
            }

            // Collect root signature info from reflection.
            auto result = RootSignatureGenerator::collectInfoFromReflection(
                dynamic_cast<DirectXRenderer*>(getRenderer())->getD3dDevice(), pReflection);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto err = std::get<Error>(std::move(result));
                err.addCurrentLocationToErrorStack();
                return err;
            }
            mtxRootSignatureInfo.second = std::get<RootSignatureGenerator::CollectedInfo>(std::move(result));
        }

        return {};
    }
} // namespace ne
