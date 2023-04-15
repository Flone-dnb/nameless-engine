#include "materials/hlsl/HlslShader.h"

// Standard.
#include <fstream>
#include <climits>
#include <filesystem>

// Custom.
#include "io/Logger.h"
#include "materials/hlsl/RootSignatureGenerator.h"
#include "misc/Globals.h"
#include "render/directx/DirectXRenderer.h"
#include "materials/ShaderFilesystemPaths.hpp"
#include "game/nodes/MeshNode.h"

// External.
#include "DirectXShaderCompiler/inc/d3d12shader.h"
#pragma comment(lib, "dxcompiler.lib")

namespace ne {
    HlslShader::HlslShader(
        Renderer* pRenderer,
        std::filesystem::path pathToCompiledShader,
        const std::string& sShaderName,
        ShaderType shaderType,
        const std::string& sSourceFileHash)
        : Shader(pRenderer, std::move(pathToCompiledShader), sShaderName, shaderType, sSourceFileHash) {
        static_assert(
            sizeof(MeshVertex) == 32, // NOLINT: current size
            "`vShaderVertexDescription` needs to be updated");
    }

    std::vector<D3D12_INPUT_ELEMENT_DESC> HlslShader::getShaderInputElementDescription() {
        return vShaderVertexDescription;
    }

    std::string HlslShader::getVertexShaderModel() { return sVertexShaderModel; }

    std::string HlslShader::getPixelShaderModel() { return sPixelShaderModel; }

    std::string HlslShader::getComputeShaderModel() { return sComputeShaderModel; }

    std::optional<Error> HlslShader::testIfShaderCacheIsCorrupted() {
        std::scoped_lock guard(mtxCompiledBlobRootSignature.first);
        auto optionalError = loadShaderDataFromDiskIfNotLoaded();
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        releaseShaderDataFromMemoryIfLoaded(true);

        return {};
    }

    std::variant<std::shared_ptr<Shader>, std::string, Error> HlslShader::compileShader(
        Renderer* pRenderer,
        const std::filesystem::path& cacheDirectory,
        const std::string& sConfiguration,
        const ShaderDescription& shaderDescription) {
        // Check that the renderer is DirectX renderer.
        if (dynamic_cast<DirectXRenderer*>(pRenderer) == nullptr) {
            return Error("the specified renderer is not a DirectX renderer");
        }

        // Check that file exists.
        if (!std::filesystem::exists(shaderDescription.pathToShaderFile)) {
            return Error(std::format(
                "the specified shader file {} does not exist", shaderDescription.pathToShaderFile.string()));
        }

        // Calculate source file hash.
        const auto sSourceFileHash = ShaderDescription::getShaderSourceFileHash(
            shaderDescription.pathToShaderFile, shaderDescription.sShaderName);
        if (sSourceFileHash.empty()) {
            return Error(std::format(
                "unable to calculate shader source file hash (shader path: \"{}\")",
                shaderDescription.pathToShaderFile.string()));
        }

        // Create compiler and utils.
        ComPtr<IDxcUtils> pUtils;
        ComPtr<IDxcCompiler3> pCompiler;
        HRESULT hResult = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
        if (FAILED(hResult)) {
            return Error(hResult);
        }
        hResult = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler));
        if (FAILED(hResult)) {
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
            sShaderModel = stringToWstring(sVertexShaderModel);
            break;
        case ShaderType::PIXEL_SHADER:
            sShaderModel = stringToWstring(sPixelShaderModel);
            break;
        case ShaderType::COMPUTE_SHADER:
            sShaderModel = stringToWstring(sComputeShaderModel);
            break;
        }

        // Create shader cache directory if needed.
        if (!std::filesystem::exists(cacheDirectory)) {
            std::filesystem::create_directory(cacheDirectory);
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
        vArgs.push_back(L"-Fd");
        auto shaderPdbPath = cacheDirectory / ShaderFilesystemPaths::getShaderCacheBaseFileName();
        shaderPdbPath += sConfiguration;
        shaderPdbPath += ".pdb";
        vArgs.push_back(shaderPdbPath);
#else
        vArgs.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
#endif

        // Add macros.
        for (const auto& macroDefine : shaderDescription.vDefinedShaderMacros) {
            vArgs.push_back(L"-D");
            vArgs.push_back(stringToWstring(macroDefine));
        }

        // Open source file.
        ComPtr<IDxcBlobEncoding> pSource = nullptr;
        hResult = pUtils->LoadFile(shaderDescription.pathToShaderFile.c_str(), nullptr, &pSource);
        if (FAILED(hResult)) {
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
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // See if errors occurred.
        ComPtr<IDxcBlobUtf8> pErrors = nullptr;
        hResult = pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
        if (FAILED(hResult)) {
            return Error(hResult);
        }
        if (pErrors != nullptr && pErrors->GetStringLength() > 0) {
            return std::string{pErrors->GetStringPointer(), pErrors->GetStringLength()};
        }

        // See if the compilation failed.
        pResults->GetStatus(&hResult);
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Get reflection.
        ComPtr<IDxcBlob> pReflectionData;
        hResult = pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr);
        if (FAILED(hResult)) {
            return Error(hResult);
        }
        if (pReflectionData == nullptr) {
            return Error("failed to get reflection data");
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

        // Generate root signature.
        auto result = RootSignatureGenerator::generate(
            dynamic_cast<DirectXRenderer*>(pRenderer)->getD3dDevice(), pReflection);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }
        // Ignore root signature (will be later used from cache), but use other results.
        auto generatedRootSignature = std::get<RootSignatureGenerator::Generated>(std::move(result));

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
        if (!shaderCacheFile.is_open()) {
            return Error(std::format("failed to save shader bytecode at {}", pathToCompiledShader.string()));
        }
        shaderCacheFile.write(
            static_cast<char*>(pCompiledShaderBlob->GetBufferPointer()),
            static_cast<long long>(pCompiledShaderBlob->GetBufferSize()));
        shaderCacheFile.close();

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
        // Save pdb file.
        ComPtr<IDxcBlob> pShaderPdb = nullptr;
        ComPtr<IDxcBlobUtf16> pShaderPdbName = nullptr;
        pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pShaderPdb), &pShaderPdbName);
        if (pShaderPdb == nullptr) {
            return Error(
                std::format("no PDB was generated for {}", shaderDescription.pathToShaderFile.string()));
        }

        std::ofstream shaderPdbFile(shaderPdbPath, std::ios::binary);
        if (!shaderPdbFile.is_open()) {
            return Error(std::format("failed to save shader PDB at {}", shaderPdbPath.string()));
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
            sSourceFileHash);
        // `generatedRootSignature.pRootSignature` is intentionally left unused
        // here we only care about the fact that it was successfully generated.
        std::scoped_lock rootSignatureInfoGuard(pShader->mtxRootSignatureInfo.first);
        pShader->mtxRootSignatureInfo.second.rootParameterIndices =
            std::move(generatedRootSignature.rootParameterIndices);
        pShader->mtxRootSignatureInfo.second.vStaticSamplers =
            std::move(generatedRootSignature.vStaticSamplers);
        pShader->mtxRootSignatureInfo.second.vRootParameters =
            std::move(generatedRootSignature.vRootParameters);
        return pShader;
    }

    std::variant<ComPtr<IDxcBlob>, Error> HlslShader::getCompiledBlob() {
        std::scoped_lock guard(mtxCompiledBlobRootSignature.first);

        auto optionalError = loadShaderDataFromDiskIfNotLoaded();
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError.value();
        }

        return mtxCompiledBlobRootSignature.second.first;
    }

    std::pair<std::mutex, HlslShader::RootSignatureInfo>* HlslShader::getRootSignatureInfo() {
        return &mtxRootSignatureInfo;
    }

    bool HlslShader::releaseShaderDataFromMemoryIfLoaded(bool bLogOnlyErrors) {
        std::scoped_lock guard(mtxCompiledBlobRootSignature.first);

        // Release shader bytecode.
        if (mtxCompiledBlobRootSignature.second.first != nullptr) {
            const auto iNewRefCount = mtxCompiledBlobRootSignature.second.first.Reset();
            if (iNewRefCount != 0) {
                Logger::get().error(
                    std::format(
                        "shader \"{}\" bytecode was requested to be released from the "
                        "memory but it's still being referenced (new ref count: {})",
                        getShaderName(),
                        iNewRefCount),
                    sHlslShaderLogCategory);
            } else if (!bLogOnlyErrors) {
                Logger::get().info(
                    std::format(
                        "shader \"{}\" bytecode is being released from the memory as it's no longer being "
                        "used (new ref count: {})",
                        getShaderName(),
                        iNewRefCount),
                    sHlslShaderLogCategory);
            }

            notifyShaderBytecodeReleasedFromMemory();
        }

        // Release root signature.
        if (mtxCompiledBlobRootSignature.second.second != nullptr) {
            const auto iNewRefCount = mtxCompiledBlobRootSignature.second.second.Reset();
            if (iNewRefCount != 0) {
                Logger::get().error(
                    std::format(
                        "shader \"{}\" root signature was requested to be released from the "
                        "memory but it's still being referenced (new ref count: {})",
                        getShaderName(),
                        iNewRefCount),
                    sHlslShaderLogCategory);
            } else if (!bLogOnlyErrors) {
                Logger::get().info(
                    std::format(
                        "shader \"{}\" root signature is being released from the memory as it's no longer "
                        "being used (new ref count: {})",
                        getShaderName(),
                        iNewRefCount),
                    sHlslShaderLogCategory);
            }
        }

        return false;
    }

    std::variant<ComPtr<IDxcBlob>, Error>
    HlslShader::readBlobFromDisk(const std::filesystem::path& pathToFile) {
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

    std::optional<Error> HlslShader::loadShaderDataFromDiskIfNotLoaded() {
        std::scoped_lock guard(mtxCompiledBlobRootSignature.first);

        auto pathResult = getPathToCompiledShader();
        if (std::holds_alternative<Error>(pathResult)) {
            auto err = std::get<Error>(std::move(pathResult));
            err.addEntry();
            return err;
        }

        const auto pathToCompiledShader = std::get<std::filesystem::path>(pathResult);

        if (mtxCompiledBlobRootSignature.second.first == nullptr) {
            // Load cached bytecode from disk.
            auto result = readBlobFromDisk(pathToCompiledShader);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }

            mtxCompiledBlobRootSignature.second.first = std::get<ComPtr<IDxcBlob>>(std::move(result));

            notifyShaderBytecodeLoadedIntoMemory();
        }

        if (mtxCompiledBlobRootSignature.second.second == nullptr) {
            // Load shader reflection from disk.
            const auto pathToShaderReflection =
                pathToCompiledShader.string() + sShaderReflectionFileExtension;

            auto blobResult = readBlobFromDisk(pathToShaderReflection);
            if (std::holds_alternative<Error>(blobResult)) {
                auto err = std::get<Error>(std::move(blobResult));
                err.addEntry();
                return err;
            }

            auto pReflectionData = std::get<ComPtr<IDxcBlob>>(std::move(blobResult));

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

            // Generate root signature.
            auto result = RootSignatureGenerator::generate(
                dynamic_cast<DirectXRenderer*>(getUsedRenderer())->getD3dDevice(), pReflection);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }

            auto generated = std::get<RootSignatureGenerator::Generated>(std::move(result));

            // Save results.
            mtxCompiledBlobRootSignature.second.second = std::move(generated.pRootSignature);
            std::scoped_lock rootSignatureInfoGuard(mtxRootSignatureInfo.first);
            mtxRootSignatureInfo.second.rootParameterIndices = std::move(generated.rootParameterIndices);
            mtxRootSignatureInfo.second.vStaticSamplers = std::move(generated.vStaticSamplers);
            mtxRootSignatureInfo.second.vRootParameters = std::move(generated.vRootParameters);
        }

        return {};
    }
} // namespace ne
