#pragma once

// Standard.
#include <variant>
#include <optional>
#include <unordered_map>
#include <stdexcept>
#include <set>

// Custom.
#include "misc/Error.h"

// External.
#include "directx/d3dx12.h"
#include <DirectXShaderCompiler/inc/d3d12shader.h>

// OS.
#include <wrl/client.h>

namespace ne {
    class HlslShader;
    class Renderer;

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
            enum class Type { CBV, SRV, UAV };

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
             * @param iDescriptorCount Number of descriptors this parameter stores.
             */
            RootParameter(
                UINT iBindPoint, UINT iSpace, Type type, bool bIsTable = false, UINT iDescriptorCount = 1);

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

            /** Descriptors in this parameter. */
            UINT iDescriptorCount = 0;

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
         * @param pRenderer     Current renderer.
         * @param pDevice       DirectX device.
         * @param pVertexShader Vertex shader.
         * @param pPixelShader  Pixel shader.
         *
         * @return Error if something went wrong, otherwise generated root signature.
         */
        static std::variant<Generated, Error> generate(
            Renderer* pRenderer, ID3D12Device* pDevice, HlslShader* pVertexShader, HlslShader* pPixelShader);

        /**
         * Generates a new root signature using the compute shader info.
         *
         * @remark Expects that root signature information is already collected for
         * shader (see @ref collectInfoFromReflection), otherwise returns error.
         *
         * @param pRenderer      Current renderer.
         * @param pDevice        DirectX device.
         * @param pComputeShader Compute shader.
         *
         * @return Error if something went wrong, otherwise generated root signature.
         */
        static std::variant<Generated, Error>
        generate(Renderer* pRenderer, ID3D12Device* pDevice, HlslShader* pComputeShader);

        /**
         * Returns index of the root parameter that points to `cbuffer` with frame constants.
         *
         * @return Root parameter index.
         */
        static constexpr UINT getFrameConstantBufferRootParameterIndex() {
            return iFrameConstantBufferRootParameterIndex;
        }

        /**
         * Returns index of the root parameter that points to `cbuffer` with general lighting constants.
         *
         * @return Root parameter index.
         */
        static constexpr UINT getGeneralLightingConstantBufferRootParameterIndex() {
            return iGeneralLightingConstantBufferRootParameterIndex;
        }

        /**
         * Returns index of the root parameter that points to a shader resource that stores point lights.
         *
         * @return Root parameter index.
         */
        static constexpr UINT getPointLightsBufferRootParameterIndex() {
            return iPointLightsBufferRootParameterIndex;
        }

        /**
         * Returns index of the root parameter that points to a shader resource that stores directional
         * lights.
         *
         * @return Root parameter index.
         */
        static constexpr UINT getDirectionalLightsBufferRootParameterIndex() {
            return iDirectionalLightsBufferRootParameterIndex;
        }

        /**
         * Returns index of the root parameter that points to a shader resource that stores spotlights.
         *
         * @return Root parameter index.
         */
        static constexpr UINT getSpotlightsBufferRootParameterIndex() {
            return iSpotlightsBufferRootParameterIndex;
        }

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
         * Adds a `Texture2D` shader resource to root parameters.
         *
         * @param vRootParameters      Parameters to add the new resource to.
         * @param rootParameterIndices Map to add new parameter to.
         * @param resourceDescription  Shader resource description.
         *
         * @return Error if something went wrong.
         */
        static std::optional<Error> addTexture2DRootParameter(
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

        /** Name of the `cbuffer` resource used to store frame data in HLSL shaders. */
        static inline const std::string sFrameConstantBufferName = "frameData";

        /** Index of the root parameter that points to `cbuffer` with frame constants. */
        static constexpr UINT iFrameConstantBufferRootParameterIndex = 0;

        /** Index of the root parameter that points to `cbuffer` with general lighting constants. */
        static constexpr UINT iGeneralLightingConstantBufferRootParameterIndex = 1;

        /** Index of the root parameter that points to a shader resource that stores point lights. */
        static constexpr UINT iPointLightsBufferRootParameterIndex = 2;

        /** Index of the root parameter that points to a shader resource that stores directional lights. */
        static constexpr UINT iDirectionalLightsBufferRootParameterIndex = 3;

        /** Index of the root parameter that points to a shader resource that stores spotlights. */
        static constexpr UINT iSpotlightsBufferRootParameterIndex = 4;
    };
} // namespace ne
