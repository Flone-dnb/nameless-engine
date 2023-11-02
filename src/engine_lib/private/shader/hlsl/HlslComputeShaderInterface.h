#pragma once

// Custom.
#include "shader/ComputeShaderInterface.h"

// External.
#include "directx/d3dx12.h"

namespace ne {
    class ID3D12Fence;

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
         * Adds a dispatch command to the specified command list to execute this compute shader.
         *
         * @warning Expects that PSO and root signature are set.
         *
         * @param pCommandList Graphics command list.
         */
        inline void dispatchOnGraphicsQueue(ID3D12GraphicsCommandList* pCommandList) {
            // TODO: set resources

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
         */
        HlslComputeShaderInterface(
            Renderer* pRenderer, const std::string& sComputeShaderName, bool bRunBeforeFrameRendering = true);
    };
}
