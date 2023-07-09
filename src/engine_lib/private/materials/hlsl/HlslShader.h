#pragma once

// Standard.
#include <mutex>

// Custom.
#include "materials/Shader.h"
#include "materials/ShaderPack.h"
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
        /** Stores information about root signature. */
        struct RootSignatureInfo {
            /** Root parameters that were used in creation of the root signature. */
            std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters;

            /** Static samplers that were used in creation of the root signature. */
            std::vector<CD3DX12_STATIC_SAMPLER_DESC> vStaticSamplers;

            /**
             * Stores pairs of `shader resource name` (taken from shader file) -
             * `root parameter index / root parameter`.
             */
            std::unordered_map<std::string, std::pair<UINT, CD3DX12_ROOT_PARAMETER>> rootParameterIndices;
        };

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
            const std::string& sSourceFileHash);

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
         * Returns used vertex shader model version.
         *
         * @return Vertex shader model version.
         */
        static std::string getVertexShaderModel();

        /**
         * Returns used pixel shader model version.
         *
         * @return Pixel shader model version.
         */
        static std::string getPixelShaderModel();

        /**
         * Returns used compute shader model version.
         *
         * @return Compute shader model version.
         */
        static std::string getComputeShaderModel();

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
         * @return One of the three values: compiled shader, string containing shader compilation
         * error/warning or an internal error.
         */
        static std::variant<std::shared_ptr<Shader>, std::string, Error> compileShader(
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
         * Returns information about generated root signature for this shader.
         *
         * @return Empty if root signature was not generated yet, use @ref getCompiledBlob
         * to generated and load everything, otherwise root signature info.
         */
        std::pair<std::mutex, std::optional<RootSignatureInfo>>* getRootSignatureInfo();

        /**
         * Releases underlying shader data (bytecode, root signature, etc.) from memory (this object will not
         * be deleted) if the shader data was loaded into memory. Next time this shader will be needed the
         * data will be loaded from disk.
         *
         * @return `false` if was released from memory, `true` if was not loaded in memory previously.
         */
        virtual bool releaseShaderDataFromMemoryIfLoaded() override;

    protected:
        /**
         * Used to save data of shader language specific (additional) shader compilation results
         * (such as reflection data, i.e. if there are some other compilation results besides compiled
         * shader bytecode which is automatically hashed and checked)
         * to later check them in @ref checkCachedAdditionalCompilationResultsInfo.
         *
         * @param cacheMetadataConfigManager Config manager to write the data to.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        saveAdditionalCompilationResultsInfo(ConfigManager& cacheMetadataConfigManager) override;

        /**
         * Used to check cached data of shader language specific (additional) shader compilation results
         * (such as reflection data, i.e. if there are some other compilation results besides compiled
         * shader bytecode which is automatically hashed and checked) whether its valid or not.
         *
         * @param cacheMetadataConfigManager Config manager to write the data to.
         * @param cacheInvalidationReason     Will be not empty if cache was invalidated
         * (i.e. cache can't be used).
         *
         * @return Error if some internal error occurred.
         */
        [[nodiscard]] virtual std::optional<Error> checkCachedAdditionalCompilationResultsInfo(
            ConfigManager& cacheMetadataConfigManager,
            std::optional<ShaderCacheInvalidationReason>& cacheInvalidationReason) override;

    private:
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
         * Looks for reflection file next to the compiled shader bytecode file
         * and calculates its hash.
         *
         * @return Error if something went wrong, otherwise hash of the reflection file.
         */
        std::variant<std::string, Error> calculateReflectionFileHash();

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

        /**
         * Contains information about @ref mtxCompiledBlobRootSignature.
         *
         * @remark Might not be calculated yet if root signature is not generated,
         * see @ref loadShaderDataFromDiskIfNotLoaded for calculating root signature information.
         */
        std::pair<std::mutex, std::optional<RootSignatureInfo>> mtxRootSignatureInfo;

        /** Name of the category used for logging. */
        inline static const char* sHlslShaderLogCategory = "HLSL Shader";

        /** Shader file encoding. */
        static inline UINT iShaderFileCodepage = DXC_CP_ACP;

        /** File extension for saving shader reflection data. */
        static inline auto sShaderReflectionFileExtension = ".reflection";

        /** Name of the key used to store reflection file hash in the metadata file. */
        static inline const auto sReflectionFileHashKeyName = "reflection_file_hash";

        /** Name of the section used to store HLSL specific metadata. */
        static inline const auto sHlslSectionName = "HLSL";

        /** Shader input element description. */
        static inline std::vector<D3D12_INPUT_ELEMENT_DESC> vShaderVertexDescription = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL",
             0,
             DXGI_FORMAT_R32G32B32_FLOAT,
             0,
             12, // NOLINT
             D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
             0},
            {"UV",
             0,
             DXGI_FORMAT_R32G32_FLOAT,
             0,
             24, // NOLINT
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
