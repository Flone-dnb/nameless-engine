#pragma once

// Standard.
#include <variant>
#include <optional>
#include <unordered_map>

// Custom.
#include "misc/Error.h"

// External.
#include "directx/d3dx12.h"
#include <DirectXShaderCompiler/inc/d3d12shader.h>

// OS.
#include <wrl/client.h>

namespace ne {
    class HlslShader;

    using namespace Microsoft::WRL;

    /**
     * Generates Root Signature based on HLSL code.
     */
    class RootSignatureGenerator {
    public:
        RootSignatureGenerator() = delete;
        RootSignatureGenerator(const RootSignatureGenerator&) = delete;
        RootSignatureGenerator& operator=(const RootSignatureGenerator&) = delete;

        /** Contains generated root signature data. */
        struct Generated {
            /** Generated root signature. */
            ComPtr<ID3D12RootSignature> pRootSignature;

            /** Static samplers of root signature. */
            std::vector<CD3DX12_STATIC_SAMPLER_DESC> vStaticSamplers;

            /** Root parameters that were used in creation of the root signature. */
            std::vector<CD3DX12_ROOT_PARAMETER> vRootParameters;

            /**
             * Stores pairs of `shader resource name` - `root parameter index / root parameter`,
             * allows determining what resource is binded to what root parameter index
             * (by using resource name taken from shader file).
             */
            std::unordered_map<std::string, std::pair<UINT, CD3DX12_ROOT_PARAMETER>> rootParameterIndices;
        };

        /** Contains data that was generated during the process of merging two root signatures. */
        struct Merged {
            /** Merged (new) root signature. */
            ComPtr<ID3D12RootSignature> pRootSignature;

            /**
             * New root parameters map of @ref pRootSignature.
             *
             * Stores pairs of `shader resource name` - `root parameter index`,
             * allows determining what resource is binded to what root parameter index
             * (by using resource name taken from shader file).
             */
            std::unordered_map<std::string, UINT> rootParameterIndices;
        };

        /**
         * Generates root signature based on HLSL code reflection.
         *
         * @param pDevice           DirectX device.
         * @param pShaderReflection Reflection from compiled HLSL shader.
         *
         * @return Error if something was wrong, otherwise generated root signature with used parameters.
         */
        static std::variant<Generated, Error>
        generate(ID3D12Device* pDevice, const ComPtr<ID3D12ShaderReflection>& pShaderReflection);

        /**
         * Merges vertex and pixel shader root signatures into one new root signature
         * that can be used in pipeline state object.
         *
         * @param pDevice       DirectX device.
         * @param pVertexShader Vertex shader.
         * @param pPixelShader  Pixel shader.
         *
         * @remark Shaders must be compiled from one shader source file.
         *
         * @return Error if something went wrong, otherwise generated root signature.
         */
        static std::variant<Merged, Error>
        merge(ID3D12Device* pDevice, HlslShader* pVertexShader, HlslShader* pPixelShader);

        /**
         * Returns index of the root parameter that points to `cbuffer` with frame constants.
         *
         * @return Root parameter index.
         */
        static inline UINT getFrameConstantBufferRootParameterIndex() {
            return iFrameConstantBufferRootParameterIndex;
        }

    private:
        /**
         * Finds static sampler for the specified sampler resource.
         *
         * @param samplerResourceDescription HLSL resource description.
         *
         * @return Error if static sampler is not found, otherwise found static sampler.
         */
        static std::variant<CD3DX12_STATIC_SAMPLER_DESC, Error>
        findStaticSamplerForSamplerResource(const D3D12_SHADER_INPUT_BIND_DESC& samplerResourceDescription);

        /**
         * Checks that essential shader resources such as frame and object constant buffer
         * are binded to the expected registers.
         *
         * @param vResourcesDescription Description of all shader resources.
         * @param vRootParameters       Final root parameter set used for root signature creation.
         * It might be reordered to put some constant buffers in the beginning (like frame/object buffers).
         *
         * @return Error if something went wrong (resource binded incorrectly).
         */
        static std::optional<Error> makeSureEssentialConstantBuffersBindedCorrectly(
            const std::vector<D3D12_SHADER_INPUT_BIND_DESC>& vResourcesDescription,
            std::vector<CD3DX12_ROOT_PARAMETER>& vRootParameters);

        /**
         * Adds a new pair of `resource name` - `root parameter index` to the specified map,
         * additionally checks if a resource with this name already exists in the map and returns
         * error in this case.
         *
         * @param mapToAddTo          Map to add new pair to.
         * @param sResourceName       Resource name.
         * @param iRootParameterIndex Root parameter index of the resource.
         * @param parameter           Parameter to add.
         *
         * @return Error if a resource with this name already exists in the map.
         */
        static std::optional<Error> addUniquePairResourceNameRootParameterIndex(
            std::unordered_map<std::string, std::pair<UINT, CD3DX12_ROOT_PARAMETER>>& mapToAddTo,
            const std::string& sResourceName,
            UINT iRootParameterIndex,
            const CD3DX12_ROOT_PARAMETER& parameter);

        /**
         * Adds a `cbuffer` shader resource to root parameters.
         *
         * @param vRootParameters      Parameters to add the new resource to.
         * @param rootParameterIndices Map to add new parameter to.
         * @param resourceDescription  Shader resource description.
         *
         * @return Error if something went wrong.
         */
        static std::optional<Error> addCbufferRootParameter(
            std::vector<CD3DX12_ROOT_PARAMETER>& vRootParameters,
            std::unordered_map<std::string, std::pair<UINT, CD3DX12_ROOT_PARAMETER>>& rootParameterIndices,
            const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription);

        /** Name of the `cbuffer` resource used to store frame data in HLSL shaders. */
        static inline const std::string sFrameConstantBufferName = "frameData";

        /** Index of the root parameter that points to `cbuffer` with frame constants. */
        static inline const UINT iFrameConstantBufferRootParameterIndex = 0;
    };
} // namespace ne
