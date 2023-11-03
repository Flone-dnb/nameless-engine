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
         * Adds a dispatch command to the specified command buffer to execute this compute shader.
         *
         * @warning Expects that pipeline and descriptor layout are set.
         *
         * @param pCommandBuffer Command buffer to add dispatch command to.
         */
        inline void dispatchOnGraphicsQueue(VkCommandBuffer pCommandBuffer) {
            // TODO: set resources

            vkCmdDispatch(
                pCommandBuffer, getThreadGroupCountX(), getThreadGroupCountY(), getThreadGroupCountZ());
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
        GlslComputeShaderInterface(
            Renderer* pRenderer, const std::string& sComputeShaderName, bool bRunBeforeFrameRendering = true);
    };
}
