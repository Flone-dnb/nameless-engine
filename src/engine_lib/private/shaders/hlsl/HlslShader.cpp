#include "shaders/hlsl/HlslShader.h"

// STL.
#include <fstream>
#include <climits>

// DXC
#include "dxc/d3d12shader.h"
#pragma comment(lib, "dxcompiler.lib")

namespace ne {
    HlslShader::HlslShader(std::filesystem::path pathToCompiledShader)
        : IShader(std::move(pathToCompiledShader)) {}

    std::variant<std::unique_ptr<IShader>, std::string, Error>
    HlslShader::compileShader(const ShaderDescription& shaderDescription) {
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
        pUtils->CreateDefaultIncludeHandler(&pIncludeHandler);

        // Create shader model string.
        std::wstring sShaderModel;
        switch (shaderDescription.shaderType) {
        case ShaderType::VERTEX_SHADER:
            sShaderModel = sVertexShaderModel;
            break;
        case ShaderType::PIXEL_SHADER:
            sShaderModel = sPixelShaderModel;
            break;
        case ShaderType::COMPUTE_SHADER:
            sShaderModel = sComputeShaderModel;
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

        for (const auto& macroDefine : shaderDescription.vDefinedShaderMacros) {
            auto macro = std::wstring(macroDefine.begin(), macroDefine.end());
            vArgs.push_back(L"-D");
            vArgs.push_back(macro.c_str());
        }

        // Open source file.
        ComPtr<IDxcBlobEncoding> pSource = nullptr;
        pUtils->LoadFile(shaderDescription.pathToShaderFile.c_str(), nullptr, &pSource);
        DxcBuffer sourceShaderBuffer{};
        sourceShaderBuffer.Ptr = pSource->GetBufferPointer();
        sourceShaderBuffer.Size = pSource->GetBufferSize();
        sourceShaderBuffer.Encoding = DXC_CP_ACP;

        // Compile it with specified arguments.
        ComPtr<IDxcResult> pResults;
        pCompiler->Compile(
            &sourceShaderBuffer,
            vArgs.data(),
            static_cast<UINT32>(vArgs.size()),
            pIncludeHandler.Get(),
            IID_PPV_ARGS(&pResults));

        // See if errors occurred.
        ComPtr<IDxcBlobUtf8> pErrors = nullptr;
        pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
        if (pErrors && pErrors->GetStringLength()) {
            return std::string{pErrors->GetStringPointer(), pErrors->GetStringLength()};
        }

        // See if the compilation failed.
        pResults->GetStatus(&hResult);
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        // Save shader binary.
        ComPtr<IDxcBlob> pCompiledShaderBlob = nullptr;
        ComPtr<IDxcBlobUtf16> pShaderName = nullptr;
        pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pCompiledShaderBlob), &pShaderName);
        if (!pCompiledShaderBlob) {
            return Error(std::format(
                "no shader binary was generated for {}", shaderDescription.pathToShaderFile.string()));
        }

        // Write bytecode to file.
        const auto pathToCompiledShader = shaderCacheDir / shaderDescription.sShaderName;
        std::ofstream shaderCacheFile(pathToCompiledShader, std::ios::binary);
        if (!shaderCacheFile.is_open()) {
            return Error(std::format("failed to save shader bytecode at {}", pathToCompiledShader.string()));
        }
        shaderCacheFile.write(
            static_cast<char*>(pCompiledShaderBlob->GetBufferPointer()),
            static_cast<long long>(pCompiledShaderBlob->GetBufferSize()));
        shaderCacheFile.close();

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
        return std::make_unique<HlslShader>(pathToCompiledShader);
    }

    std::variant<ComPtr<IDxcBlob>, Error> HlslShader::getCompiledBlob() {
        // Load cached bytecode from disk.
        const auto pathToCompiledShader = getPathToCompiledShader();

        std::ifstream shaderBytecodeFile(pathToCompiledShader, std::ios::binary);

        // Get file size.
        shaderBytecodeFile.seekg(0, std::ios::end);
        const long long iFileByteCount = shaderBytecodeFile.tellg();
        shaderBytecodeFile.seekg(0, std::ios::beg);

        if (iFileByteCount > static_cast<long long>(UINT_MAX)) {
            shaderBytecodeFile.close();
            return Error("shader cache file is too big");
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

        ComPtr<IDxcBlob> pCompiledBlob;
        hResult = pUtils->CreateBlob(
            pBlobData.get(),
            static_cast<unsigned int>(iBlobSize),
            DXC_CP_ACP,
            reinterpret_cast<IDxcBlobEncoding**>(pCompiledBlob.GetAddressOf()));
        if (FAILED(hResult)) {
            return Error(hResult);
        }

        return pCompiledBlob;
    }
} // namespace ne
