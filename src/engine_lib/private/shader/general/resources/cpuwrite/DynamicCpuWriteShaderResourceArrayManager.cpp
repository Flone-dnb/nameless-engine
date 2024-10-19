#include "DynamicCpuWriteShaderResourceArrayManager.h"

// Custom.
#include "io/Logger.h"
#include "shader/general/resources/cpuwrite/DynamicCpuWriteShaderResourceArray.h"
#include "shader/general/resources/cpuwrite/ShaderCpuWriteResourceBinding.h"
#include "misc/Profiler.hpp"

namespace ne {

    DynamicCpuWriteShaderResourceArrayManager::DynamicCpuWriteShaderResourceArrayManager(
        GpuResourceManager* pResourceManager)
        : pResourceManager(pResourceManager) {}

    std::variant<std::unique_ptr<DynamicCpuWriteShaderResourceArraySlot>, Error>
    DynamicCpuWriteShaderResourceArrayManager::reserveSlotsInArray(ShaderCpuWriteResourceBinding* pShaderResource) {
        std::scoped_lock guard(mtxCpuWriteShaderResourceArrays.first);

        // Check if we already have an array for resources with this name.
        auto it = mtxCpuWriteShaderResourceArrays.second.find(pShaderResource->getShaderResourceName());
        if (it == mtxCpuWriteShaderResourceArrays.second.end()) {
            // Create a new array for this resource.
            auto result = DynamicCpuWriteShaderResourceArray::create(
                pResourceManager,
                pShaderResource->getShaderResourceName(),
                pShaderResource->getResourceDataSizeInBytes());
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            auto pCreatedArray =
                std::get<std::unique_ptr<DynamicCpuWriteShaderResourceArray>>(std::move(result));
            auto pCreatedArrayRaw = pCreatedArray.get();

            // Save.
            mtxCpuWriteShaderResourceArrays.second[pShaderResource->getShaderResourceName()] =
                std::move(pCreatedArray);

            // Calculate еру total size of all arrays now.
            size_t iTotalSizeInBytes = 0;
            for (const auto& [sArrayName, pArray] : mtxCpuWriteShaderResourceArrays.second) {
                iTotalSizeInBytes += pArray->getSizeInBytes();
            }

            // Log creation.
            Logger::get().info(std::format(
                "created a new CPU-write array (with capacity step size {}) to handle the data of the shader "
                "CPU write resource \"{}\" (CPU-write arrays now in total: {} and their total size: {})",
                pCreatedArrayRaw->getCapacityStepSize(),
                pShaderResource->getShaderResourceName(),
                mtxCpuWriteShaderResourceArrays.second.size(),
                formatBytesToMegabytes(iTotalSizeInBytes)));

            // Update iterator.
            it = mtxCpuWriteShaderResourceArrays.second.find(pShaderResource->getShaderResourceName());
        }

        // Make sure this array's element size is equal to the requested one.
        if (it->second->getElementSize() != pShaderResource->getResourceDataSizeInBytes()) [[unlikely]] {
            // This is probably a different resource with a non-unique name.
            // We operate only on resource names here because once an array is being resized it
            // updates all descriptors (of all pipelines) which are used for a specific resource
            // name to reference a new (resized) VkBuffer.
            return Error(std::format(
                "shader resource \"{}\" requested to reserve a memory slot with size {} bytes in an array "
                "and a memory manager already has an array for handling slots of shader resources with name "
                "\"{}\" but this array's element size is {} bytes not {} bytes, this might mean that you "
                "have 2 different shaders with 2 different resources (in size) but both resources in both "
                "shaders have the same name which is an error, if this is the case, please rename one of "
                "the resources",
                pShaderResource->getShaderResourceName(),
                pShaderResource->getResourceDataSizeInBytes(),
                pShaderResource->getShaderResourceName(),
                it->second->getElementSize(),
                pShaderResource->getResourceDataSizeInBytes()));
        }

        // Insert a new slot to the new array.
        auto result = it->second->insert(pShaderResource);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // After inserting (maybe the first) slot, check for empty arrays.
        removeEmptyArrays();

        return std::get<std::unique_ptr<DynamicCpuWriteShaderResourceArraySlot>>(std::move(result));
    }

    DynamicCpuWriteShaderResourceArray* DynamicCpuWriteShaderResourceArrayManager::getArrayForShaderResource(
        const std::string& sShaderResourceName) {
        std::scoped_lock guard(mtxCpuWriteShaderResourceArrays.first);

        // Find an array that handles the specified shader resource name.
        const auto it = mtxCpuWriteShaderResourceArrays.second.find(sShaderResourceName);
        if (it == mtxCpuWriteShaderResourceArrays.second.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    void DynamicCpuWriteShaderResourceArrayManager::removeEmptyArrays() {
        std::scoped_lock guard(mtxCpuWriteShaderResourceArrays.first);

        // Erase empty arrays.
        std::erase_if(mtxCpuWriteShaderResourceArrays.second, [](auto& item) {
            // Because insertion only happens from the manager and the mutex is locked array's size
            // will not change.
            return item.second->getSize() == 0;
        });
    }

    std::string DynamicCpuWriteShaderResourceArrayManager::formatBytesToMegabytes(size_t iSizeInBytes) {
        return std::format("{:.4F} MB",
                           static_cast<float>(iSizeInBytes) / 1024.0F / 1024.0F); // NOLINT
    }

    DynamicCpuWriteShaderResourceArrayManager::~DynamicCpuWriteShaderResourceArrayManager() {
        std::scoped_lock guard(mtxCpuWriteShaderResourceArrays.first);

        removeEmptyArrays();

        // Self check: make sure all arrays were deleted, we expect all arrays to be deleted before the
        // renderer is destroyed, otherwise this means that some array is not empty for some reason.
        if (!mtxCpuWriteShaderResourceArrays.second.empty()) [[unlikely]] {
            // Get names of non-empty arrays.
            std::string sNonEmptyArrayNames;
            for (const auto& [sArrayName, pArray] : mtxCpuWriteShaderResourceArrays.second) {
                if (pArray == nullptr) [[unlikely]] {
                    Logger::get().error(
                        std::format("found not erased `nullptr` array with the name \"{}\"", sArrayName));
                    continue;
                }
                sNonEmptyArrayNames += std::format("- {} (size: {})\n", sArrayName, pArray->getSize());
            }

            // Show an error.
            Error error(std::format(
                "array resource array manager is being destroyed but there are still {} array(s) "
                "exist:\n{}",
                mtxCpuWriteShaderResourceArrays.second.size(),
                sNonEmptyArrayNames));
            error.showError();
            // don't throw in destructor, just quit
        }
    }

}
