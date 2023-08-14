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
        std::scoped_lock guard(mtxGlslShaderCpuWriteResources.first);

        // Erase empty arrays.
        std::erase_if(mtxGlslShaderCpuWriteResources.second, [](auto& item) {
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
        std::scoped_lock guard(mtxGlslShaderCpuWriteResources.first);

        removeEmptyArrays();

        // Self check: make sure all storage arrays were deleted,
        // we expect all arrays to be deleted before the renderer is destroyed,
        // otherwise this means that some array is not empty for some reason.
        if (!mtxGlslShaderCpuWriteResources.second.empty()) [[unlikely]] {
            // Get names of non-empty arrays.
            std::string sNonEmptyArrayNames;
            for (const auto& [sArrayName, pArray] : mtxGlslShaderCpuWriteResources.second) {
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
                mtxGlslShaderCpuWriteResources.second.size(),
                sNonEmptyArrayNames));
            error.showError();
            // don't throw in destructor, just quit
        }
    }

    std::variant<std::unique_ptr<VulkanStorageResourceArraySlot>, Error>
    VulkanStorageResourceArrayManager::reserveSlotsInArray(GlslShaderCpuWriteResource* pShaderResource) {
        std::scoped_lock guard(mtxGlslShaderCpuWriteResources.first);

        // Check if we already have a storage array for resources with this name.
        auto it = mtxGlslShaderCpuWriteResources.second.find(pShaderResource->getResourceName());
        if (it == mtxGlslShaderCpuWriteResources.second.end()) {
            // Create a new array for this resource.
            auto result = VulkanStorageResourceArray::create(
                pResourceManager,
                pShaderResource->getResourceName(),
                pShaderResource->getOriginalResourceSizeInBytes(),
                FrameResourcesManager::getFrameResourcesCount()); // because we insert
                                                                  // `getFrameResourcesCount` slots at once
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            mtxGlslShaderCpuWriteResources.second[pShaderResource->getResourceName()] =
                std::get<std::unique_ptr<VulkanStorageResourceArray>>(std::move(result));

            // Calculate total size of all arrays now.
            size_t iTotalSizeInBytes = 0;
            for (const auto& [sArrayName, pArray] : mtxGlslShaderCpuWriteResources.second) {
                iTotalSizeInBytes += pArray->getSizeInBytes();
            }

            // Log creation.
            Logger::get().info(fmt::format(
                "created a new storage array to handle \"{}\" shader CPU write resource data "
                "(storage arrays now in total: {}, total size: {})",
                pShaderResource->getResourceName(),
                mtxGlslShaderCpuWriteResources.second.size(),
                formatBytesToMegabytes(iTotalSizeInBytes)));

            // Update iterator.
            it = mtxGlslShaderCpuWriteResources.second.find(pShaderResource->getResourceName());
        }

        // Make sure this array's element size is equal to the requested one.
        if (it->second->getElementSize() != pShaderResource->getOriginalResourceSizeInBytes()) [[unlikely]] {
            // This is probably a different resource with a non-unique name.
            // We operate only on resource names here because once an array is being resized it
            // updates all descriptors (of all pipelines) which are used for a specific resource
            // name to reference a new (resized) VkBuffer.
            return Error(fmt::format(
                "shader resource \"{}\" requested to reserve a memory slot with size {} bytes in an array "
                "and a memory manager already has an array for handling slots of shader resources with name "
                "\"{}\" but this array's element size is {} bytes not {} bytes, this might mean that you "
                "have 2 different shaders with 2 different resources (in size) but both resources in both "
                "shaders have the same name which is an error, if this is the case, please rename one of "
                "the resources",
                pShaderResource->getResourceName(),
                pShaderResource->getOriginalResourceSizeInBytes(),
                pShaderResource->getResourceName(),
                it->second->getElementSize(),
                pShaderResource->getOriginalResourceSizeInBytes()));
        }

        // Insert a new slot.
        auto result = it->second->insert(pShaderResource);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        removeEmptyArrays();

        return std::get<std::unique_ptr<VulkanStorageResourceArraySlot>>(std::move(result));
    }

    std::optional<Error> VulkanStorageResourceArrayManager::bindDescriptorsToRecreatedPipelineResources(
        VulkanRenderer* pVulkanRenderer) {
        std::scoped_lock guard(mtxGlslShaderCpuWriteResources.first);

        // Update descriptors.
        for (const auto& [sArrayName, pArray] : mtxGlslShaderCpuWriteResources.second) {
            auto optionalError = pArray->updateDescriptors(pVulkanRenderer);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    std::string VulkanStorageResourceArrayManager::formatBytesToMegabytes(size_t iSizeInBytes) {
        return fmt::format(
            "{:.4F} MB",
            static_cast<float>(iSizeInBytes) / 1024.0F / 1024.0F); // NOLINT: magic numbers
    }

} // namespace ne
