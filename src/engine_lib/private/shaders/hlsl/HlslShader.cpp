﻿#include "shaders/hlsl/HlslShader.h"

// STL.
#include <fstream>
#include <climits>

// External.
#include "DirectXShaderCompiler/inc/d3d12shader.h"
#pragma comment(lib, "dxcompiler.lib")

// Custom.
#include "io/Logger.h"
#include "shaders/hlsl/RootSignatureGenerator.h"
#include "misc/Globals.h"
#include "render/directx/DirectXRenderer.h"

namespace ne {
    HlslShader::HlslShader(
        IRenderer* pRenderer,
        std::filesystem::path pathToCompiledShader,
        const std::string& sShaderName,
        ShaderType shaderType)
        : IShader(pRenderer, std::move(pathToCompiledShader), sShaderName, shaderType) {}

    std::variant<std::shared_ptr<IShader>, std::string, Error>
    HlslShader::compileShader(IRenderer* pRenderer, const ShaderDescription& shaderDescription) {
        // Check that the renderer is DirectX renderer.
        if (!dynamic_cast<DirectXRenderer*>(pRenderer)) {
            return Error("the specified renderer is not a DirectX renderer");
        }

        // Check that file exists.
        if (!std::filesystem::exists(shaderDescription.pathToShaderFile)) {
            return Error(std::format(
                "the specified shader file {} does not exist", shaderDescription.pathToShaderFile.string()));
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

        const auto shaderCacheDir = getPathToShaderCacheDirectory();

        // Convert std::string to std::wstring to be used.
        std::wstring sShaderEntry(
            shaderDescription.sShaderEntryFunctionName.begin(),
            shaderDescription.sShaderEntryFunctionName.end());

        // Prepare compilation arguments.
        std::vector<LPCWSTR> vArgs;
        vArgs.push_back(shaderDescription.pathToShaderFile.c_str());
        vArgs.push_back(L"-E");
        vArgs.push_back(sShaderEntry.c_str());
        vArgs.push_back(L"-T");
        vArgs.push_back(sShaderModel.c_str());
#if defined(DEBUG)
        vArgs.push_back(DXC_ARG_DEBUG);
        vArgs.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
        vArgs.push_back(L"-Fd");
        auto shaderPdbPath = shaderCacheDir / shaderDescription.sShaderName;
        shaderPdbPath += ".pdb";
        vArgs.push_back(shaderPdbPath.wstring().c_str());
#else
        vArgs.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
#endif

        // Add macros.
        for (const auto& macroDefine : shaderDescription.vDefinedShaderMacros) {
            auto macro = std::wstring(macroDefine.begin(), macroDefine.end());
            vArgs.push_back(L"-D");
            vArgs.push_back(macro.c_str());
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

        // Compile it with specified arguments.
        ComPtr<IDxcResult> pResults;
        hResult = pCompiler->Compile(
            &sourceShaderBuffer,
            vArgs.data(),
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
        if (pErrors && pErrors->GetStringLength()) {
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
        auto result = RootSignatureGenerator::generateRootSignature(
            dynamic_cast<DirectXRenderer*>(pRenderer)->pDevice, pReflection);
        if (std::holds_alternative<Error>(result)) {
            auto err = std::get<Error>(std::move(result));
            err.addEntry();
            return err;
        }

        // Get compiled shader binary.
        ComPtr<IDxcBlob> pCompiledShaderBlob = nullptr;
        ComPtr<IDxcBlobUtf16> pShaderName = nullptr;
        pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pCompiledShaderBlob), &pShaderName);
        if (!pCompiledShaderBlob) {
            return Error(std::format(
                "no shader binary was generated for {}", shaderDescription.pathToShaderFile.string()));
        }

        // Write shader bytecode to file.
        const auto pathToCompiledShader = shaderCacheDir / shaderDescription.sShaderName;
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
            (shaderCacheDir / shaderDescription.sShaderName).string() + sShaderReflectionFileExtension;
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
        if (!pShaderPdb) {
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

        // Return shader instance.
        return std::make_shared<HlslShader>(
            pRenderer, pathToCompiledShader, shaderDescription.sShaderName, shaderDescription.shaderType);
    }

    std::variant<ComPtr<IDxcBlob>, Error> HlslShader::getCompiledBlob() {
        std::scoped_lock guard1(mtxCompiledBlob.first);
        if (!mtxCompiledBlob.second) {
            // Load cached bytecode from disk.
            const auto pathToCompiledShader = getPathToCompiledShader();

            auto result = readBlobFromDisk(pathToCompiledShader);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }

            mtxCompiledBlob.second = std::get<ComPtr<IDxcBlob>>(std::move(result));
        }

        std::scoped_lock guard2(mtxRootSignature.first);
        if (!mtxRootSignature.second) {
            // Load shader reflection from disk.
            const auto pathToShaderReflection =
                getPathToCompiledShader().string() + sShaderReflectionFileExtension;

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
            auto result = RootSignatureGenerator::generateRootSignature(
                dynamic_cast<DirectXRenderer*>(getUsedRenderer())->pDevice, pReflection);
            if (std::holds_alternative<Error>(result)) {
                auto err = std::get<Error>(std::move(result));
                err.addEntry();
                return err;
            }

            mtxRootSignature.second = std::get<ComPtr<ID3D12RootSignature>>(std::move(result));
        }

        return mtxCompiledBlob.second;
    }

    bool HlslShader::releaseBytecodeFromMemoryIfLoaded() {
        {
            // Release shader bytecode.
            std::scoped_lock guard(mtxCompiledBlob.first);

            if (!mtxCompiledBlob.second) {
                return true;
            }

            const auto iNewRefCount = mtxCompiledBlob.second.Reset();
            if (iNewRefCount != 0) {
                Logger::get().error(
                    std::format(
                        "THIS IS A BUG, REPORT TO DEVELOPERS: shader \"{}\" bytecode was released from "
                        "memory but it's still being referenced (new ref count: {})",
                        getShaderName(),
                        iNewRefCount),
                    "");
            } else {
                Logger::get().info(
                    std::format(
                        "shader \"{}\" bytecode is being released from memory as it's no longer being used "
                        "(new ref count: {})",
                        getShaderName(),
                        iNewRefCount),
                    "");
            }
        }

        {
            // Release root signature.
            std::scoped_lock guard(mtxRootSignature.first);

            if (!mtxRootSignature.second) {
                return true;
            }

            const auto iNewRefCount = mtxRootSignature.second.Reset();
            if (iNewRefCount != 0) {
                Logger::get().error(
                    std::format(
                        "THIS IS A BUG, REPORT TO DEVELOPERS: shader \"{}\" root signature was released from "
                        "memory but it's still being referenced (new ref count: {})",
                        getShaderName(),
                        iNewRefCount),
                    "");
            } else {
                Logger::get().info(
                    std::format(
                        "shader \"{}\" root signature is being released from memory as it's no longer being "
                        "used (new ref count: {})",
                        getShaderName(),
                        iNewRefCount),
                    "");
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
} // namespace ne
