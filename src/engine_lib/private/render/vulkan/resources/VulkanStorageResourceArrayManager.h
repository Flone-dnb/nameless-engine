#pragma once

// Standard.
#include <variant>
#include <memory>
#include <array>
#include <mutex>
#include <string>
#include <unordered_map>

// Custom.
#include "render/general/resources/FrameResourcesManager.h"

namespace ne {
    class VulkanRenderer;
    class VulkanStorageResourceArray;
    class VulkanStorageResourceArraySlot;
    class GlslShaderCpuWriteResource;
    class VulkanRenderer;

    /** Manages arrays of resources of various CPU write shader resources. */
    class VulkanStorageResourceArrayManager {
        // Only resource manager is supposed to own this manager.
        friend class VulkanResourceManager;

    public:
        VulkanStorageResourceArrayManager(const VulkanStorageResourceArrayManager&) = delete;
        VulkanStorageResourceArrayManager& operator=(const VulkanStorageResourceArrayManager&) = delete;

        VulkanStorageResourceArrayManager() = delete;

        ~VulkanStorageResourceArrayManager();

        /**
         * Requests a new slot in the storage buffer array to be reserved for use by the specified
         * shader resource.
         *
         * @remark There is no public `erase` function because slot destruction automatically
         * uses internal `erase`, see documentation on the returned slot object.
         *
         * @param pShaderResource Shader resource that requires a slot in the array.
         * If the array will resize the specified resource (if it has an active slot in the array)
         * will be marked as "needs update" through the shader resource manager.
         *
         * @return Error if something went wrong, otherwise slot of the newly added element in
         * the array.
         */
        std::variant<std::unique_ptr<VulkanStorageResourceArraySlot>, Error>
        reserveSlotsInArray(GlslShaderCpuWriteResource* pShaderResource);

        /**
         * Updates descriptors in all pipelines to make descriptors reference the underlying VkBuffer of used
         * arrays.
         *
         * @warning Expects that the GPU is not doing any work and that no new frames are being submitted now.
         *
         * @remark Generally called after all pipeline resources were re-created to update re-created
         * descriptors.
         *
         * @param pVulkanRenderer Vulkan renderer.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        bindDescriptorsToRecreatedPipelineResources(VulkanRenderer* pVulkanRenderer);

    private:
        /**
         * Formats the specified size in bytes to the following format: "<number> MB",
         * for example the number 1512 will be formatted to the following text: "0.0014 MB".
         *
         * @param iSizeInBytes Size in bytes to format.
         *
         * @return Formatted text.
         */
        static std::string formatBytesToMegabytes(size_t iSizeInBytes);

        /**
         * Resource manager that owns this manager
         *
         * @param pResourceManager Owner manager.
         */
        VulkanStorageResourceArrayManager(VulkanResourceManager* pResourceManager);

        /** Goes through all arrays in @ref mtxGlslShaderCpuWriteResources and removed empty ones. */
        void removeEmptyArrays();

        /** Do not delete (free) this pointer. Resource manager that owns this manager. */
        VulkanResourceManager* pResourceManager = nullptr;

        /**
         * Stores pairs of "shader resource name" - "data of shader resources",
         * where "shader resource name" is the name of the resource written in the GLSL file.
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<std::string, std::unique_ptr<VulkanStorageResourceArray>>>
            mtxGlslShaderCpuWriteResources;
    };
} // namespace ne
