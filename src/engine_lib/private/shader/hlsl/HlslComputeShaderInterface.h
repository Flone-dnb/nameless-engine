#pragma once

// Standard.
#include <unordered_map>

// Custom.
#include "shader/ComputeShaderInterface.h"
#include "render/directx/resources/DirectXResource.h"

// External.
#include "directx/d3dx12.h"

namespace ne {
    /** Interface to configure and run an HLSL compute shader. */
    class HlslComputeShaderInterface : public ComputeShaderInterface {
        // Only parent class can create instances of this class because there are some specific things
        // we need to do when creating objects of this class and parent class handles these things.
        friend class ComputeShaderInterface;

    public:
        HlslComputeShaderInterface() = delete;
        virtual ~HlslComputeShaderInterface() override = default;

        HlslComputeShaderInterface(const HlslComputeShaderInterface&) = delete;
        HlslComputeShaderInterface& operator=(const HlslComputeShaderInterface&) = delete;

        /**
         * Binds the specified resource to be available in compute shaders.
         *
         * @warning This overload is used in cases where you cannot transfer resource ownership to the
         * compute shader interface. In this case you must guarantee that the resource will not be deleted
         * while this compute shader interface exists and while the GPU is processing this compute shader.
         *
         * @param pResource           Resource to bind to compute shader.
         * @param sShaderResourceName Resource name from shader.
         * @param usage               Resource usage.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindResource(
            GpuResource* pResource,
            const std::string& sShaderResourceName,
            ComputeResourceUsage usage) override;

        /**
         * Adds a dispatch command to the specified command list to execute this compute shader.
         *
         * @warning Expects that PSO and root signature are set.
         *
         * @param pCommandList Graphics command list.
         */
        inline void dispatchOnGraphicsQueue(ID3D12GraphicsCommandList* pCommandList) {
            // Bind CBV resources.
            for (const auto& [iRootParameterIndex, pResource] : cbvResources) {
                pCommandList->SetComputeRootConstantBufferView(
                    iRootParameterIndex, pResource->getInternalResource()->GetGPUVirtualAddress());
            }

            // Bind UAV resources.
            for (const auto& [iRootParameterIndex, pResource] : uavResources) {
                pCommandList->SetComputeRootUnorderedAccessView(
                    iRootParameterIndex, pResource->getInternalResource()->GetGPUVirtualAddress());
            }

            // Bind SRV resources.
            for (const auto& [iRootParameterIndex, pResource] : srvResources) {
                pCommandList->SetComputeRootShaderResourceView(
                    iRootParameterIndex, pResource->getInternalResource()->GetGPUVirtualAddress());
            }

            // Add a dispatch command.
            pCommandList->Dispatch(getThreadGroupCountX(), getThreadGroupCountY(), getThreadGroupCountZ());
        }

    protected:
        /**
         * Initializes the interface.
         *
         * @param pRenderer                Used renderer.
         * @param sComputeShaderName       Name of the compiled compute shader to use.
         * @param bRunBeforeFrameRendering Determines whether this compute shader should run before frame
         * rendering or after a frame is rendered on the GPU. Only valid when using graphics queue.
         * @param executionGroup           Determines execution group of this shader where shaders of the
         * first group will be executed before shaders from the second group and so on.
         */
        HlslComputeShaderInterface(
            Renderer* pRenderer,
            const std::string& sComputeShaderName,
            bool bRunBeforeFrameRendering,
            ComputeExecutionGroup executionGroup);

        /** Stores pairs of "root parameter index" - "resource to bind as CBV". */
        std::unordered_map<UINT, DirectXResource*> cbvResources;

        /** Stores pairs of "root parameter index" - "resource to bind as UAV". */
        std::unordered_map<UINT, DirectXResource*> uavResources;

        /** Stores pairs of "root parameter index" - "resource to bind as SRV". */
        std::unordered_map<UINT, DirectXResource*> srvResources;
    };
}
