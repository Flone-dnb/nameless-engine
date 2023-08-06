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
        virtual ~VulkanFrameResource() override = default;

        /** Stores recorded commands. */
        VkCommandBuffer pCommandBuffer = nullptr;

    private:
        /**
         * Called by frame resource manager after a frame resource was constructed to initialize its fields.
         *
         * @param pRenderer Used renderer.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> initialize(Renderer* pRenderer) override;
    };
} // namespace ne
