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
    class GlslShaderCpuReadWriteResource;

    /** Manages arrays of resources of various CPU read/write shader resources. */
    class VulkanStorageResourceArrayManager {
        // Only resource manager is supposed to own this manager.
        friend class VulkanResourceManager;

    public:
        VulkanStorageResourceArrayManager(const VulkanStorageResourceArrayManager&) = delete;
        VulkanStorageResourceArrayManager& operator=(const VulkanStorageResourceArrayManager&) = delete;

        VulkanStorageResourceArrayManager() = delete;

        ~VulkanStorageResourceArrayManager();

    private:
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
        reserveSlotsInArray(GlslShaderCpuReadWriteResource* pShaderResource);

        /**
         * Resource manager that owns this manager
         *
         * @param pResourceManager Owner manager.
         */
        VulkanStorageResourceArrayManager(VulkanResourceManager* pResourceManager);

        /** Goes through all arrays in @ref mtxGlslShaderCpuReadWriteResources and removed empty ones. */
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
            mtxGlslShaderCpuReadWriteResources;
    };
} // namespace ne
