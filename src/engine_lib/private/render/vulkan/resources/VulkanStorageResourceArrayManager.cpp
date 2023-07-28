#include "VulkanStorageResourceArrayManager.h"

// Custom.
#include "render/vulkan/resources/VulkanStorageResourceArray.h"
#include "materials/glsl/GlslShaderResource.h"
#include "render/vulkan/resources/VulkanResourceManager.h"

namespace ne {

    VulkanStorageResourceArrayManager::VulkanStorageResourceArrayManager(
        VulkanResourceManager* pResourceManager) {
        this->pResourceManager = pResourceManager;
    }

    void VulkanStorageResourceArrayManager::removeEmptyArrays() {
        std::scoped_lock guard(mtxGlslShaderCpuReadWriteResources.first);

        // Erase empty arrays.
        std::erase_if(mtxGlslShaderCpuReadWriteResources.second, [](auto& item) {
            // Because insertion only happens from the manager and the mutex is locked array's size
            // will not change.
            if (item.second->getSize() == 0) {
                Logger::get().info(
                    fmt::format("deleting storage array \"{}\" because it's empty", item.first));
                return true;
            }
            return false;
        });
    }

    VulkanStorageResourceArrayManager::~VulkanStorageResourceArrayManager() {
        std::scoped_lock guard(mtxGlslShaderCpuReadWriteResources.first);

        removeEmptyArrays();

        // Self check: make sure all storage arrays were deleted,
        // we expect all arrays to be deleted before the renderer is destroyed,
        // otherwise this means that some array is not empty for some reason.
        if (!mtxGlslShaderCpuReadWriteResources.second.empty()) [[unlikely]] {
            // Get names of non-empty arrays.
            std::string sNonEmptyArrayNames;
            for (const auto& [sArrayName, pArray] : mtxGlslShaderCpuReadWriteResources.second) {
                if (pArray == nullptr) [[unlikely]] {
                    Logger::get().error(
                        fmt::format("found not erased `nullptr` array with the name \"{}\"", sArrayName));
                    continue;
                }
                sNonEmptyArrayNames += fmt::format("- {} (size: {})\n", sArrayName, pArray->getSize());
            }

            // Show an error.
            Error error(fmt::format(
                "storage resource array manager is being destroyed but there are still {} array(s) "
                "exist:\n{}",
                mtxGlslShaderCpuReadWriteResources.second.size(),
                sNonEmptyArrayNames));
            error.showError();
            // don't throw in destructor, just quit
        }
    }

    std::variant<std::unique_ptr<VulkanStorageResourceArraySlot>, Error>
    VulkanStorageResourceArrayManager::reserveSlotsInArray(GlslShaderCpuReadWriteResource* pShaderResource) {
        std::scoped_lock guard(mtxGlslShaderCpuReadWriteResources.first);

        // Check if we already have a storage array for resources with this name.
        auto it = mtxGlslShaderCpuReadWriteResources.second.find(pShaderResource->getResourceName());
        if (it == mtxGlslShaderCpuReadWriteResources.second.end()) {
            // Create a new array for this resource.
            auto result = VulkanStorageResourceArray::create(
                pResourceManager,
                pShaderResource->getResourceName(), // use resource names as array names
                pShaderResource->getOriginalResourceSizeInBytes(),
                FrameResourcesManager::getFrameResourcesCount()); // because we insert
                                                                  // `getFrameResourcesCount` slots at once
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                return error;
            }
            mtxGlslShaderCpuReadWriteResources.second[pShaderResource->getResourceName()] =
                std::get<std::unique_ptr<VulkanStorageResourceArray>>(std::move(result));

            // Log creation.
            Logger::get().info(fmt::format(
                "created a new storage array to handle \"{}\" shader CPU read/write resource data "
                "(total storage arrays now: {})",
                pShaderResource->getResourceName(),
                mtxGlslShaderCpuReadWriteResources.second.size()));

            // Update iterator.
            it = mtxGlslShaderCpuReadWriteResources.second.find(pShaderResource->getResourceName());
        }

        // Insert a new slot.
        auto result = it->second->insert(pShaderResource);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }

        removeEmptyArrays();

        return std::get<std::unique_ptr<VulkanStorageResourceArraySlot>>(std::move(result));
    }

} // namespace ne
