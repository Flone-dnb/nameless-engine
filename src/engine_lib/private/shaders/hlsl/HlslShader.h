#pragma once

// STL.
#include <mutex>

// Custom.
#include "shaders/Shader.h"
#include "shaders/ShaderPack.h"
#include "render/Renderer.h"

// External.
#include "directx/d3dx12.h"
#include "DirectXShaderCompiler/inc/dxcapi.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    /**
     * Represents compiled HLSL shader.
     */
    class HlslShader : public Shader {
    public:
        /**
         * Constructor. Used to create shader using cache.
         *
         * @param pRenderer            Used renderer.
         * @param pathToCompiledShader Path to compiled shader blob on disk.
         * @param sShaderName          Unique name of this shader.
         * @param shaderType           Type of this shader.
         * @param sSourceFileHash      Shader source file hash, used to tell what shaders were compiled from
         * the same file.
         */
        HlslShader(
            Renderer* pRenderer,
            std::filesystem::path pathToCompiledShader,
            const std::string& sShaderName,
            ShaderType shaderType,
            std::string sSourceFileHash);

        HlslShader() = delete;
        HlslShader(const HlslShader&) = delete;
        HlslShader& operator=(const HlslShader&) = delete;

        virtual ~HlslShader() override = default;

        /**
         * Returns shader input layout description (@ref vShaderVertexDescription).
         *
         * @return Input layout description.
         */
        static std::vector<D3D12_INPUT_ELEMENT_DESC> getShaderInputElementDescription();

        /**
         * Tests if shader cache for this shader is corrupted or not.
         *
         * @remark This function should be used if you want to use shader cache.
         *
         * @return Error if shader cache is corrupted, empty is OK.
         */
        virtual std::optional<Error> testIfShaderCacheIsCorrupted() override;

        /**
         * Compiles a shader.
         *
         * @param pRenderer         DirectX renderer.
         * @param cacheDirectory    Directory to store this shader's cache,
         * for example: ".../shader_cache/engine.default".
         * @param sConfiguration    Shader configuration text that will be added to the name.
         * @param shaderDescription Description that describes the shader and how the shader should be
         * compiled.
         *
         * @return Returns one of the three values:
         * @arg compiled shader
         * @arg string containing shader compilation error/warning
         * @arg internal error
         */
        static std::variant<
            std::shared_ptr<Shader> /** Compiled shader pack. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(
            Renderer* pRenderer,
            const std::filesystem::path& cacheDirectory,
            const std::string& sConfiguration,
            const ShaderDescription& shaderDescription);

        /**
         * Loads compiled bytecode from disk and stores it in memory.
         * Subsequent calls to this function will just copy the bytecode pointer
         * (no disk loading will happen).
         *
         * @return Compiled shader blob.
         */
        std::variant<ComPtr<IDxcBlob>, Error> getCompiledBlob();

        /**
         * Returns root parameters that were used for generating shader's root signature.
         *
         * @return Root parameters.
         */
        std::vector<CD3DX12_ROOT_PARAMETER> getShaderRootParameters() const;

        /**
         * Returns static samplers that were used for generating shader's root signature.
         *
         * @return Static samplers.
         */
        std::vector<CD3DX12_STATIC_SAMPLER_DESC> getShaderStaticSamplers() const;

        /**
         * Releases underlying shader data (bytecode, root signature, etc.) from memory (this object will not
         * be deleted) if the shader data was loaded into memory. Next time this shader will be needed the
         * data will be loaded from disk.
         *
         * @param bLogOnlyErrors Specify `true` to only log errors, `false` to log errors and info.
         * Specifying `true` is useful when we are testing if shader cache is corrupted or not,
         * to make the log slightly cleaner.
         *
         * @return `false` if was released from memory, `true` if was not loaded in memory previously.
         */
        virtual bool releaseShaderDataFromMemoryIfLoaded(bool bLogOnlyErrors = false) override;

    private:
        friend class ShaderManager;

        /**
         * Reads file and creates a new DXC blob using file's content.
         *
         * @param pathToFile Path to the file to create blob from.
         *
         * @return Error if something went wrong, otherwise created blob.
         */
        static std::variant<ComPtr<IDxcBlob>, Error>
        readBlobFromDisk(const std::filesystem::path& pathToFile);

        /**
         * Loads shader data (bytecode, root signature, etc.) from disk cache to @ref
         * mtxCompiledBlobRootSignature (if something is not loaded).
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> loadShaderDataFromDiskIfNotLoaded();

        /**
         * Mutex for read/write operations on compiled blob and shader's root signature.
         * Compiled shader bytecode and root signature (may be empty if not stored in memory right now).
         */
        std::pair<std::recursive_mutex, std::pair<ComPtr<IDxcBlob>, ComPtr<ID3D12RootSignature>>>
            mtxCompiledBlobRootSignature;

        /** Root parameters that were used for generating @ref mtxCompiledBlobRootSignature. */
        std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters;

        /** Static samplers that were used for generating @ref mtxCompiledBlobRootSignature. */
        std::vector<CD3DX12_STATIC_SAMPLER_DESC> vStaticSamplers;

        /** Shader file encoding. */
        static inline UINT iShaderFileCodepage = DXC_CP_ACP;

        /** File extension for saving shader reflection data. */
        static inline auto sShaderReflectionFileExtension = ".reflection";

        /** Shader input element description. */
        static inline std::vector<D3D12_INPUT_ELEMENT_DESC> vShaderVertexDescription = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"CUSTOM",
             0,
             DXGI_FORMAT_R32G32B32A32_FLOAT,
             0,
             32,
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
             0}};

        // -------------------------------------------------------------------------
        // ! if adding new shader models add them to cache config in ShaderManager !
        // -------------------------------------------------------------------------
        /** Used vertex shader model. */
        static inline std::string sVertexShaderModel = "vs_6_0";
        /** Used pixel shader model. */
        static inline std::string sPixelShaderModel = "ps_6_0";
        /** Used compute shader model. */
        static inline std::string sComputeShaderModel = "cs_6_0";
        // -------------------------------------------------------------------------
        // ! if adding new shader models add them to cache config in ShaderManager !
        // -------------------------------------------------------------------------
    };
} // namespace ne
