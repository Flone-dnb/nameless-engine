#include "VulkanStorageResourceArray.h"

// Custom.
#include "render/general/resources/GpuResourceManager.h"
#include "render/Renderer.h"
#include "materials/glsl/GlslShaderResource.h"

namespace ne {

    std::variant<std::unique_ptr<VulkanStorageResourceArray>, Error> VulkanStorageResourceArray::create(
        GpuResourceManager* pResourceManager,
        const std::string& sArrayName,
        size_t iElementSizeInBytes,
        size_t iCapacityStepSizeMultiplier) {
        // Calculate capacity step size.
        auto result = calculateCapacityStepSize(iElementSizeInBytes, iCapacityStepSizeMultiplier);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        const auto iCapacityStepSize = std::get<size_t>(result);

        // Self check: make sure capacity step is not zero.
        if (iCapacityStepSize == 0) [[unlikely]] {
            return Error(fmt::format("calculated capacity step size is 0 (array {})", sArrayName));
        }

        // Self check: make sure capacity step is even because we use INT / INT.
        if (iCapacityStepSize % 2 != 0) [[unlikely]] {
            return Error(fmt::format(
                "calculated capacity step size ({}) is not even (array {})", iCapacityStepSize, sArrayName));
        }

        return std::unique_ptr<VulkanStorageResourceArray>(new VulkanStorageResourceArray(
            pResourceManager, sArrayName, iElementSizeInBytes, iCapacityStepSize));
    }

    VulkanStorageResourceArray::VulkanStorageResourceArray(
        GpuResourceManager* pResourceManager,
        const std::string& sArrayName,
        size_t iElementSizeInBytes,
        size_t iCapacityStepSize)
        : iCapacityStepSize(iCapacityStepSize), sName(sArrayName), iElementSizeInBytes(iElementSizeInBytes) {
        this->pResourceManager = pResourceManager;
    }

    VulkanStorageResourceArray::~VulkanStorageResourceArray() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Make sure there are no active slots.
        if (!mtxInternalResources.second.activeSlots.empty()) [[unlikely]] {
            Error error(fmt::format(
                "the storage array \"{}\" is being destroyed but it still has {} "
                "active slot(s) (this is a bug, report to developers)",
                sName,
                mtxInternalResources.second.activeSlots.size()));
            error.showError();
            return; // don't throw in destructor, just quit
        }

        // Make sure our size is zero.
        if (mtxInternalResources.second.iSize != 0) [[unlikely]] {
            Error error(fmt::format(
                "the storage array \"{}\" is being destroyed but it's not empty (size = {}) "
                "although there are no active slot(s) (this is a bug, report to developers)",
                sName,
                mtxInternalResources.second.iSize));
            error.showError();
            return; // don't throw in destructor, just quit
        }
    }

    std::variant<std::unique_ptr<VulkanStorageResourceArraySlot>, Error>
    VulkanStorageResourceArray::insert(GlslShaderCpuReadWriteResource* pShaderResource) {
        // Lock both self and shader resources manager because I think there might be
        // the following AB-BA mutex locking issue if we only lock self:
        // - [thread 1] shader resource manager is in `destroyResource` and locked his mutex
        // - [thread 2] new mesh is spawning and its shader resources now running `insert`
        // - [thread 1] shader resource manager has `erase`d some old shader resource and inside of
        // its destructor our `markSlotAsNoLongerBeingUsed` is called but this thread will have to
        // wait because thread 2 is currently using `insert`
        // - [thread 2] we found out that we need to re-create the array and expand it, we notify
        // shader resource manager that some resource needs to be marked as "needs update" but this
        // thread will have to wait because thread 1 is currently using `destroyResource`
        const auto pMtxShaderResources =
            pResourceManager->getRenderer()->getShaderCpuReadWriteResourceManager()->getResources();
        std::scoped_lock guard(mtxInternalResources.first, pMtxShaderResources->first);

        // Expand the array if needed.
        if (mtxInternalResources.second.iSize == mtxInternalResources.second.iCapacity) {
            auto optionalError = expandArray();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addEntry();
                return optionalError.value();
            }
        }

        // Get free index.
        size_t iNewIndex = 0;
        if (mtxInternalResources.second.iNextFreeArrayIndex == mtxInternalResources.second.iCapacity) {
            iNewIndex = mtxInternalResources.second.noLongerUsedArrayIndices.front();
            mtxInternalResources.second.noLongerUsedArrayIndices.pop();
        } else {
            iNewIndex = mtxInternalResources.second.iNextFreeArrayIndex;
            mtxInternalResources.second.iNextFreeArrayIndex += 1;
        }

        // Mark increased heap size.
        mtxInternalResources.second.iSize += 1;

        // Create a new slot.
        // (wrapping it into a unique ptr so that `std::move` will not change the slot object
        // and our raw pointer in `activeSlots` will still point to the correct slot)
        auto pNewSlot = std::unique_ptr<VulkanStorageResourceArraySlot>(
            new VulkanStorageResourceArraySlot(this, iNewIndex, pShaderResource));

        // Add new slot to an array of active slots.
        mtxInternalResources.second.activeSlots.insert(pNewSlot.get());

        return pNewSlot;
    }

    size_t VulkanStorageResourceArray::getSize() {
        std::scoped_lock guard(mtxInternalResources.first);
        return mtxInternalResources.second.iSize;
    }

    size_t VulkanStorageResourceArray::getCapacity() {
        std::scoped_lock guard(mtxInternalResources.first);
        return mtxInternalResources.second.iCapacity;
    }

    size_t VulkanStorageResourceArray::getElementSize() const { return iElementSizeInBytes; }

    size_t VulkanStorageResourceArray::getCapacityStepSize() const { return iCapacityStepSize; }

    std::string VulkanStorageResourceArray::formatBytesToKilobytes(size_t iSizeInBytes) {
        return fmt::format(
            "{:.1} KB",
            static_cast<float>(iSizeInBytes) /
                1024.0F); // NOLINT: magic number (size of one kilobyte in bytes)
    }

    std::variant<size_t, Error> VulkanStorageResourceArray::calculateCapacityStepSize(
        size_t iElementSizeInBytes, size_t iCapacityStepSizeMultiplier) {
        static constexpr size_t iMaxElementSizeForCapacity = 1024 * 1024 * 2; // NOLINT
        static constexpr size_t iMaxCapacityStepSize = 40;                    // NOLINT
        static constexpr size_t iMinCapacityStepSize = 2;                     // NOLINT

        // Capacity coef will be maximum at small element size and minimum at big element size.
        const auto capacityCoef = 1.0F - std::clamp(
                                             static_cast<float>(iElementSizeInBytes) /
                                                 static_cast<float>(iMaxElementSizeForCapacity),
                                             0.0F,
                                             1.0F);

        size_t iCalculatedCapacityStepSize = std::clamp(
            static_cast<size_t>(iMaxCapacityStepSize * capacityCoef),
            iMinCapacityStepSize,
            iMaxCapacityStepSize);

        if (iCalculatedCapacityStepSize % 2 != 0) {
            // Make even.
            // Because min/max are even this means that we are between min/max and we just
            // need to decide to add or remove 1.
            static_assert(iMaxCapacityStepSize % 2 == 0);
            static_assert(iMinCapacityStepSize % 2 == 0);
            if (capacityCoef > 0.5F) {
                iCalculatedCapacityStepSize += 1;
            } else {
                iCalculatedCapacityStepSize -= 1;
            }
        }

        // Make sure that the specified capacity step multiplier is bigger than 0.
        if (iCapacityStepSizeMultiplier == 0) [[unlikely]] {
            return Error("the specified capacity step size multiplier is zero");
        }

        // Make sure that the specified capacity step multiplier is equal or smaller than
        // frames in-flight.
        if (iCapacityStepSizeMultiplier > FrameResourcesManager::getFrameResourcesCount()) [[unlikely]] {
            return Error(fmt::format(
                "the specified capacity step size multiplier {} is bigger than available frame resource "
                "count {}",
                iCapacityStepSizeMultiplier,
                FrameResourcesManager::getFrameResourcesCount()));
        }

        // Multiply the capacity step size.
        iCalculatedCapacityStepSize *= iCapacityStepSizeMultiplier;

        return iCalculatedCapacityStepSize;
    }

    void VulkanStorageResourceArray::markSlotAsNoLongerBeingUsed(VulkanStorageResourceArraySlot* pSlot) {
        // Lock both self and shader resources manager because I think there might be
        // the an AB-BA mutex locking issue that I described in `insert`.
        const auto pMtxShaderResources =
            pResourceManager->getRenderer()->getShaderCpuReadWriteResourceManager()->getResources();
        std::scoped_lock guard(mtxInternalResources.first, pMtxShaderResources->first);

        // Find the specified slot in the array of active slots.
        const auto it = mtxInternalResources.second.activeSlots.find(pSlot);
        if (it == mtxInternalResources.second.activeSlots.end()) [[unlikely]] {
            Logger::get().error(fmt::format(
                "a slot with index {} has notified the storage array about no longer being used "
                "but this slot does not exist in the array of active slots",
                pSlot->iIndexInArray));
            return;
        }

        // Remove from active slots.
        mtxInternalResources.second.activeSlots.erase(it);

        // Add the unused index to the array of unused indices.
        mtxInternalResources.second.noLongerUsedArrayIndices.push(pSlot->iIndexInArray);

        // Decrement array size.
        mtxInternalResources.second.iSize -= 1;

        // Shrink the array if needed.
        if (mtxInternalResources.second.iCapacity >= iCapacityStepSize * 2 &&
            mtxInternalResources.second.iCapacity <=
                (mtxInternalResources.second.iCapacity - iCapacityStepSize - iCapacityStepSize / 2)) {
            auto optionalError = shrinkArray();
            if (optionalError.has_value()) {
                optionalError->addEntry();
                Logger::get().error(optionalError->getFullErrorMessage());
            }
        }
    }

    void VulkanStorageResourceArray::updateSlotData(VulkanStorageResourceArraySlot* pSlot, void* pData) {
        // Lock both self and shader resources manager because I think there might be
        // the an AB-BA mutex locking issue that I described in `insert`.
        const auto pMtxShaderResources =
            pResourceManager->getRenderer()->getShaderCpuReadWriteResourceManager()->getResources();
        std::scoped_lock guard(mtxInternalResources.first, pMtxShaderResources->first);

        // Copy data.
        mtxInternalResources.second.pStorageBuffer->copyDataToElement(
            pSlot->iIndexInArray, pData, iElementSizeInBytes);
    }

    std::optional<Error> VulkanStorageResourceArray::createArray(size_t iCapacity) {
        std::scoped_lock guard(mtxInternalResources.first);

        // Calculate the current and the new size in bytes.
        const auto iCurrentSizeInBytes = mtxInternalResources.second.iCapacity * iElementSizeInBytes;
        const auto iNewSizeInBytes = iCapacity * iElementSizeInBytes;

        // Log the fact that we will pause the rendering.
        Logger::get().info(std::format(
            "waiting for the GPU to finish work up to this point to (re)create the storage array "
            "\"{}\" from capacity {} ({}) to {} ({}) (current actual size: {})",
            sName,
            mtxInternalResources.second.iCapacity,
            formatBytesToKilobytes(iCurrentSizeInBytes),
            iCapacity,
            formatBytesToKilobytes(iNewSizeInBytes),
            mtxInternalResources.second.iSize));

        // Make sure we don't render anything and this array is not used by the GPU.
        const auto pRenderer = pResourceManager->getRenderer();
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // don't unlock `renderResourcesMutex` until we finished updating all slots

        // Create a new storage buffer.
        auto result = pResourceManager->createResourceWithCpuWriteAccess(
            sName, iElementSizeInBytes, iCapacity, CpuVisibleShaderResourceUsageDetails(false));
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            return error;
        }
        mtxInternalResources.second.pStorageBuffer =
            std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

        // Save the new capacity.
        mtxInternalResources.second.iCapacity = iCapacity;

        // Self check: make sure active slot count is equal to array size to avoid setting indices
        // out of bounds.
        if (mtxInternalResources.second.activeSlots.size() != mtxInternalResources.second.iSize)
            [[unlikely]] {
            return Error(fmt::format(
                "the storage array \"{}\" was recreated but its active slot count ({}) "
                "is not equal to the size ({}) (this is a bug, report to developers)",
                sName,
                mtxInternalResources.second.activeSlots.size(),
                mtxInternalResources.second.iSize));
        }

        // Get shader resource manager to be used.
        const auto pShaderResourceManager = pRenderer->getShaderCpuReadWriteResourceManager();

        // Update all active slots.
        size_t iCurrentIndex = 0;
        for (const auto& pSlot : mtxInternalResources.second.activeSlots) {
            // Save new index to slot.
            pSlot->iIndexInArray = iCurrentIndex;

            // Increment index.
            iCurrentIndex += 1;

            // Mark resource as "needs update" so that it will copy its data to the new
            // storage buffer's index.
            pShaderResourceManager->markResourceAsNeedsUpdate(pSlot->pShaderResource);
        }

        return {};
    }

    std::optional<Error> VulkanStorageResourceArray::expandArray() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Make sure the array is fully filled and there's no free space.
        if (mtxInternalResources.second.iSize != mtxInternalResources.second.iCapacity) [[unlikely]] {
            return Error(fmt::format(
                "a request to expand the array \"{}\" of capacity {} while the actual size is {} "
                "was rejected, reason: expand condition is not met (this is a bug, report to developers)",
                sName,
                mtxInternalResources.second.iCapacity,
                mtxInternalResources.second.iSize));
        }

        // Make sure there are no unused indices.
        if (!mtxInternalResources.second.noLongerUsedArrayIndices.empty()) [[unlikely]] {
            return Error(fmt::format(
                "requested to expand the array \"{}\" of capacity {} while there are not used "
                "indices exist ({}) (actual size is {}) (this is a bug, report to developers)",
                sName,
                mtxInternalResources.second.iCapacity,
                mtxInternalResources.second.noLongerUsedArrayIndices.size(),
                mtxInternalResources.second.iSize));
        }

        // Make sure our new capacity will not exceed type limit.
        constexpr auto iMaxArrayCapacity = std::numeric_limits<size_t>::max();
        if (iMaxArrayCapacity - iCapacityStepSize < mtxInternalResources.second.iCapacity) [[unlikely]] {
            return Error(fmt::format(
                "a request to expand the array \"{}\" of capacity {} was rejected, reason: "
                "array will exceed the type limit of {}",
                sName,
                mtxInternalResources.second.iCapacity,
                iMaxArrayCapacity));
        }

        // Save old array capacity to use later.
        const auto iOldArrayCapacity = mtxInternalResources.second.iCapacity;

        // Re-create the array with the new capacity.
        auto optionalError = createArray(iOldArrayCapacity + iCapacityStepSize);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addEntry();
            return optionalError.value();
        }

        // Update internal values.
        mtxInternalResources.second.iNextFreeArrayIndex = iOldArrayCapacity;
        mtxInternalResources.second.noLongerUsedArrayIndices = {};

        return {};
    }

    std::optional<Error> VulkanStorageResourceArray::shrinkArray() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Make sure we can shrink (check that we are not on the minimum capacity).
        if (mtxInternalResources.second.iCapacity < iCapacityStepSize * 2) [[unlikely]] {
            return Error(std::format(
                "a request to shrink the array \"{}\" of capacity {} with the actual size of {} was "
                "rejected, reason: need at least the size of {} to shrink (this is a bug, report "
                "to developers)",
                sName,
                mtxInternalResources.second.iCapacity,
                mtxInternalResources.second.iSize,
                iCapacityStepSize * 2));
        }

        // Only shrink if we can erase `iCapacityStepSize` unused elements and will have some
        // free space (i.e. we will not be on the edge to expand).
        if (mtxInternalResources.second.iSize >
            mtxInternalResources.second.iCapacity - iCapacityStepSize - iCapacityStepSize / 2) [[unlikely]] {
            return Error(std::format(
                "a request to shrink the array \"{}\" of capacity {} with the actual size of {} was "
                "rejected, reason: shrink condition is not met (this is a bug, report to developers)",
                sName,
                mtxInternalResources.second.iCapacity,
                mtxInternalResources.second.iSize));
        }

        // Calculate the new capacity.
        const auto iNewCapacity = mtxInternalResources.second.iCapacity - iCapacityStepSize;

        // Re-create the array with the new capacity.
        auto optionalError = createArray(iNewCapacity);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addEntry();
            return optionalError.value();
        }

        // Update internal values.
        mtxInternalResources.second.iNextFreeArrayIndex = iNewCapacity - iCapacityStepSize / 2;
        mtxInternalResources.second.noLongerUsedArrayIndices = {};

        return {};
    }

    VulkanStorageResourceArraySlot::VulkanStorageResourceArraySlot(
        VulkanStorageResourceArray* pArray,
        size_t iIndexInArray,
        GlslShaderCpuReadWriteResource* pShaderResource) {
        this->pArray = pArray;
        this->iIndexInArray = iIndexInArray;
        this->pShaderResource = pShaderResource;
    }

    VulkanStorageResourceArraySlot::~VulkanStorageResourceArraySlot() {
        pArray->markSlotAsNoLongerBeingUsed(this);
    }

    void VulkanStorageResourceArraySlot::updateData(void* pData) { pArray->updateSlotData(this, pData); }

} // namespace ne
