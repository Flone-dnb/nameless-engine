#pragma once

// Standard.
#include <variant>
#include <optional>
#include <unordered_map>
#include <stdexcept>
#include <array>
#include <set>

// Custom.
#include "misc/Error.h"
#include "SpecialRootParameterSlot.hpp"

// External.
#include "directx/d3dx12.h"
#include <DirectXShaderCompiler/inc/d3d12shader.h>

// OS.
#include <wrl/client.h>

namespace ne {
    class HlslShader;
    class DirectXRenderer;

    using namespace Microsoft::WRL;

    /** Represents a type of a sampler defined in the HLSL code. */
    enum class SamplerType {
        BASIC,     //< Usual `SamplerState` type in HLSL.
        COMPARISON //< `SamplerComparisonState` type in HLSL.
    };

    /**
     * Generates Root Signature based on HLSL code.
     */
    class RootSignatureGenerator {
    public:
        RootSignatureGenerator() = delete;
        RootSignatureGenerator(const RootSignatureGenerator&) = delete;
        RootSignatureGenerator& operator=(const RootSignatureGenerator&) = delete;

        /** Wrapper for D3D root parameter type. */
        class RootParameter {
        public:
            /** Describes a root parameter type. */
            enum class Type { CONSTANTS, CBV, SRV, UAV, SAMPLER };

            /** Creates uninitialized parameter. */
            RootParameter() = default;

            /**
             * Initializes a root parameter.
             *
             * @param iBindPoint       Register binding index.
             * @param iSpace           Register space.
             * @param type             Root parameter type.
             * @param bIsTable         `true` to initialize this parameter as descriptor table
             * (even if descriptor count is 1), otherwise `false` to initialize it as just one descriptor.
             * @param iCount           If type is table them defined the number of descriptors this parameter
             * stores, if type is constants then the number of 32 bit constants, otherwise ignored.
             */
            RootParameter(UINT iBindPoint, UINT iSpace, Type type, bool bIsTable = false, UINT iCount = 1);

            /**
             * Generates root parameter description that describes a single descriptor.
             *
             * @remark Shows error if this parameter was initialized as descriptor table.
             *
             * @return Root parameter description.
             */
            CD3DX12_ROOT_PARAMETER generateSingleDescriptorDescription() const;

            /**
             * Generates root table range description.
             *
             * @remark Shows error if this parameter was initialized as a single descriptor.
             *
             * @return Root table range description.
             */
            CD3DX12_DESCRIPTOR_RANGE generateTableRange() const;

            /**
             * Returns visibility of this parameter.
             *
             * @return Visibility.
             */
            D3D12_SHADER_VISIBILITY getVisibility() const;

            /**
             * Tells whether this parameter describes a descriptor table or just a single view.
             *
             * @return `true` if @ref generateTableRange should be used, otherwise
             * @ref generateSingleDescriptorDescription.
             */
            bool isTable() const;

        private:
            /** Binding register index. */
            UINT iBindPoint = 0;

            /** Binding register space. */
            UINT iSpace = 0;

            /** Parameter type. */
            Type type;

            /**
             * If @ref type is table them defined the number of descriptors this parameter
             * stores, if @ref type is constants then the number of 32 bit constants,
             * otherwise ignored.
             */
            UINT iCount = 0;

            /** Whether this parameter should be initialized as descriptor table or not. */
            bool bIsTable = false;

            /** Visibility of this parameter. */
            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
        };

        /** Contains collected root signature info. */
        struct CollectedInfo {
            /** Static samplers of root signature. */
            std::set<SamplerType> staticSamplers;

            /** Root parameters that were used in creation of the root signature. */
            std::vector<RootParameter> vRootParameters;

            /**
             * Stores pairs of `shader resource name` - `root parameter index / root parameter`,
             * allows determining what resource is binded to what root parameter index
             * (by using resource name taken from shader file).
             */
            std::unordered_map<std::string, std::pair<UINT, RootParameter>> rootParameterIndices;

            /**
             * Stores pairs of "name of field defined in HLSL in RootConstants cbuffer" (all with `uint` type)
             * and "offset from the beginning of the struct (in `uint`s not bytes)".
             *
             * @remark May be empty if constants are not used.
             *
             * @remark If a non `uint` fields is found an error is returned instead.
             */
            std::unordered_map<std::string, size_t> rootConstantOffsets;
        };

        /** Contains data that was generated during the process of merging two root signatures. */
        struct Generated {
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

            /**
             * Stores indices of some non-user specified root parameters. Duplicates some root parameters
             * and their indices from @ref rootParameterIndices but only stored some special non-user
             * specified root parameter indices.
             *
             * @remark Generally used for fast access (without doing a `find` in the map) to some
             * root parameter indices.
             *
             * @remark Example usage: `iRootParameterIndex = vIndices[Slot::FRAME_DATA]`.
             */
            std::array<UINT, static_cast<unsigned int>(SpecialRootParameterSlot::SIZE)>
                vSpecialRootParameterIndices;

            /**
             * Stores pairs of "name of field defined in HLSL in RootConstants cbuffer" (all with `uint`
             * type) and "offset from the beginning of the struct (in `uint`s not bytes)".
             *
             * @remark May be empty if constants are not used.
             *
             * @remark If a non `uint` fields is found an error is returned instead.
             */
            std::unordered_map<std::string, size_t> rootConstantOffsets;
        };

        /**
         * Generates root signature based on HLSL code reflection.
         *
         * @param pDevice           DirectX device.
         * @param pShaderReflection Reflection from compiled HLSL shader.
         *
         * @return Error if something was wrong, otherwise generated root signature with used parameters.
         */
        static std::variant<CollectedInfo, Error> collectInfoFromReflection(
            ID3D12Device* pDevice, const ComPtr<ID3D12ShaderReflection>& pShaderReflection);

        /**
         * Generates a new root signature using the vertex and pixel shader info.
         *
         * @remark Expects that root signature information is already collected for both
         * shaders (see @ref collectInfoFromReflection), otherwise returns error.
         *
         * @warning If a shader uses a static sampler this function will take the current texture filtering
         * setting from the RenderSettings and will set it as a static sampler. This means that
         * once the current texture filtering setting is changed you need to re-run this function
         * to set a new filter into the root signature's static sampler.
         *
         * @param pRenderer     DirectX renderer.
         * @param pVertexShader Vertex shader.
         * @param pPixelShader  Pixel shader. Specify `nullptr` to generate root signature only for vertex
         * shader.
         *
         * @return Error if something went wrong, otherwise generated root signature.
         */
        static std::variant<Generated, Error>
        generateGraphics(DirectXRenderer* pRenderer, HlslShader* pVertexShader, HlslShader* pPixelShader);

        /**
         * Generates a new root signature using the compute shader info.
         *
         * @remark Expects that root signature information is already collected for
         * shader (see @ref collectInfoFromReflection), otherwise returns error.
         *
         * @param pRenderer      DirectX renderer.
         * @param pComputeShader Compute shader.
         *
         * @return Error if something went wrong, otherwise generated root signature.
         */
        static std::variant<Generated, Error>
        generateCompute(DirectXRenderer* pRenderer, HlslShader* pComputeShader);

    private:
        /**
         * Finds static sampler for the specified sampler resource.
         *
         * @param samplerResourceDescription HLSL resource description.
         *
         * @return Error if static sampler is not found, otherwise found static sampler.
         */
        static std::variant<SamplerType, Error>
        findStaticSamplerForSamplerResource(const D3D12_SHADER_INPUT_BIND_DESC& samplerResourceDescription);

        /**
         * Adds special root signature resources (if they are actually used).
         *
         * @param shaderRootParameterIndices    Used resources of shaders.
         * @param vRootParameters               Single root parameters of the root signature that will be
         * generated.
         * @param vTableRanges                  Range root parameters of the root signature that will be
         * generated.
         * @param addedRootParameterNames       Names of the root parameters that were added.
         * @param rootParameterIndices          Pairs of "resource name" - "root parameter index" that were
         * added.
         * @param vSpecialRootParameterIndices  Indices of special root parameters.
         */
        static void addSpecialResourceRootParametersIfUsed(
            std::unordered_map<std::string, std::pair<UINT, RootSignatureGenerator::RootParameter>>&
                shaderRootParameterIndices,
            std::vector<CD3DX12_ROOT_PARAMETER>& vRootParameters,
            std::vector<CD3DX12_DESCRIPTOR_RANGE>& vTableRanges,
            std::set<std::string>& addedRootParameterNames,
            std::unordered_map<std::string, UINT>& rootParameterIndices,
            std::array<UINT, static_cast<unsigned int>(SpecialRootParameterSlot::SIZE)>&
                vSpecialRootParameterIndices);

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
            std::unordered_map<std::string, std::pair<UINT, RootParameter>>& mapToAddTo,
            const std::string& sResourceName,
            UINT iRootParameterIndex,
            const RootParameter& parameter);

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
            std::vector<RootParameter>& vRootParameters,
            std::unordered_map<std::string, std::pair<UINT, RootParameter>>& rootParameterIndices,
            const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription);

        /**
         * Adds a `(RW)Texture2D` shader resource to root parameters.
         *
         * @param vRootParameters      Parameters to add the new resource to.
         * @param rootParameterIndices Map to add new parameter to.
         * @param resourceDescription  Shader resource description.
         * @param bIsReadWrite         `true` if buffer is read/write, `false` if only read.
         *
         * @return Error if something went wrong.
         */
        static std::optional<Error> addTexture2DRootParameter(
            std::vector<RootParameter>& vRootParameters,
            std::unordered_map<std::string, std::pair<UINT, RootParameter>>& rootParameterIndices,
            const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription,
            bool bIsReadWrite);

        /**
         * Adds a `SamplerState` shader resource to root parameters.
         *
         * @param vRootParameters      Parameters to add the new resource to.
         * @param rootParameterIndices Map to add new parameter to.
         * @param resourceDescription  Shader resource description.
         *
         * @return Error if something went wrong.
         */
        static std::optional<Error> addSamplerRootParameter(
            std::vector<RootParameter>& vRootParameters,
            std::unordered_map<std::string, std::pair<UINT, RootParameter>>& rootParameterIndices,
            const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription);

        /**
         * Adds a `(RW)StructuredBuffer` shader resource to root parameters.
         *
         * @param vRootParameters      Parameters to add the new resource to.
         * @param rootParameterIndices Map to add new parameter to.
         * @param resourceDescription  Shader resource description.
         * @param bIsReadWrite         `true` if buffer is read/write, `false` if only read.
         *
         * @return Error if something went wrong.
         */
        static std::optional<Error> addStructuredBufferRootParameter(
            std::vector<RootParameter>& vRootParameters,
            std::unordered_map<std::string, std::pair<UINT, RootParameter>>& rootParameterIndices,
            const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription,
            bool bIsReadWrite);

        /**
         * Looks if the specified cbuffer resource description stores root constants (@ref
         * sRootConstantsTypeName) and if it does adds root constant offsets to the specified map.
         *
         * @param pShaderReflection    Shader reflection.
         * @param resourceDescription  Cbuffer description.
         * @param rootConstantOffsets  If found root constants, their offsets will be added here.
         * @param vRootParameters      Parameters to add the new resource to (if root constants are found).
         * @param rootParameterIndices Map to add new parameter to (if root constants are found).
         *
         * @return Error if something went wrong, otherwise `false` if no root constants were found
         * and `true` if root constants were found and variable offsets were added.
         */
        static std::variant<bool, Error> processRootConstantsIfFound(
            const ComPtr<ID3D12ShaderReflection>& pShaderReflection,
            const D3D12_SHADER_INPUT_BIND_DESC& resourceDescription,
            std::unordered_map<std::string, size_t>& rootConstantOffsets,
            std::vector<RootParameter>& vRootParameters,
            std::unordered_map<std::string, std::pair<UINT, RootParameter>>& rootParameterIndices);

        /** Name of the shader `cbuffer` that will be considered as buffer that stores root constants. */
        static inline const std::string sRootConstantsVariableName = "constants";

        /** Name of the shader struct that stores root constants. */
        static inline const std::string sRootConstantsTypeName = "RootConstants";
    };
} // namespace ne
