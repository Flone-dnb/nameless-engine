#pragma once

// Custom.
#include "shader/ComputeShaderInterface.h"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    /** Interface to configure and run a GLSL compute shader. */
    class GlslComputeShaderInterface : public ComputeShaderInterface {
        // Only parent class can create instances of this class because there are some specific things
        // we need to do when creating objects of this class and parent class handles these things.
        friend class ComputeShaderInterface;

    public:
        GlslComputeShaderInterface() = delete;
        virtual ~GlslComputeShaderInterface() override = default;

        GlslComputeShaderInterface(const GlslComputeShaderInterface&) = delete;
        GlslComputeShaderInterface& operator=(const GlslComputeShaderInterface&) = delete;

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
         * @param bUpdateOnlyCurrentFrameResourceDescriptors Specify `true` if you guarantee that you will
         * bind a different GPU resource on the next frame, specify `false` if you are not sure if you will
         * rebind the resource on the next frame or not. When `true` is specified only descriptors of the
         * current frame resource will be updated (because descriptors of other frame resources might be in
         * use and it's invalid to update them), when `false` descriptors of all frame resources will be
         * updated.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindResource(
            GpuResource* pResource,
            const std::string& sShaderResourceName,
            ComputeResourceUsage usage,
            bool bUpdateOnlyCurrentFrameResourceDescriptors = false) override;

        /**
         * Adds a dispatch command to the specified command buffer to execute this compute shader.
         *
         * @warning Expects that pipeline and descriptor layout are set.
         *
         * @param pCommandBuffer Command buffer to add dispatch command to.
         */
        inline void dispatchOnGraphicsQueue(VkCommandBuffer pCommandBuffer) {
            vkCmdDispatch(
                pCommandBuffer, getThreadGroupCountX(), getThreadGroupCountY(), getThreadGroupCountZ());
        }

    protected:
        /**
         * Initializes the interface.
         *
         * @param pRenderer                Used renderer.
         * @param sComputeShaderName       Name of the compiled compute shader to use.
         * @param executionStage           Determines when the shader will be executed.
         * @param executionGroup           Determines execution group of this shader where shaders of the
         * first group will be executed before shaders from the second group and so on.
         */
        GlslComputeShaderInterface(
            Renderer* pRenderer,
            const std::string& sComputeShaderName,
            ComputeExecutionStage executionStage,
            ComputeExecutionGroup executionGroup);
    };
}
