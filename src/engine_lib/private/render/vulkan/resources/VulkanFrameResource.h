#pragma once

// Standard.
#include <optional>

// Custom.
#include "render/general/resources/FrameResource.h"
#include "misc/Error.h"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    class Renderer;

    /** Stores objects used by one frame. */
    struct VulkanFrameResource : public FrameResource {
        virtual ~VulkanFrameResource() override;

        /** Stores recorded commands. */
        VkCommandBuffer pCommandBuffer = nullptr;

        /** Signaled when submitted commands were finished executing. */
        VkFence pFence = nullptr;

        /** Signaled when an image from the swapchain was acquired and is ready for drawing. */
        VkSemaphore pSemaphoreSwapChainImageAcquired = nullptr;

        /**
         * Signaled when submitted commands were finished executing and the image is now
         * ready for presenting.
         */
        VkSemaphore pSemaphoreSwapChainImageDrawingFinished = nullptr;

    private:
        /**
         * Called by frame resource manager after a frame resource was constructed to initialize its fields.
         *
         * @param pRenderer Used renderer.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> initialize(Renderer* pRenderer) override;

        /** Renderer that was passed to @ref initialize. */
        Renderer* pRenderer = nullptr;
    };
} // namespace ne
