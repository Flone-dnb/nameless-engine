#include "DynamicCpuWriteShaderResourceArray.h"

// Custom.
#include "render/general/resources/GpuResourceManager.h"
#include "shader/general/resources/binding/cpuwrite/ShaderCpuWriteResourceBindingManager.h"
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "shader/general/resources/binding/global/GlobalShaderResourceBindingManager.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif

namespace ne {

    DynamicCpuWriteShaderResourceArraySlot::DynamicCpuWriteShaderResourceArraySlot(
        DynamicCpuWriteShaderResourceArray* pArray,
        size_t iIndexInArray,
        ShaderCpuWriteResourceBinding* pShaderResource)
        : pArray(pArray), pShaderResource(pShaderResource) {
        // Self check:
        static_assert(
            std::is_same_v<decltype(this->iIndexInArray), unsigned int>,
            "update cast and type limit check below");

#undef max // <- this macro prevents usage of numeric_limits::max
        // Check before converting to unsigned int, see slot index docs for more info.
        if (iIndexInArray > std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            Error error(std::format(
                "received slot index {} exceeds type limit (array \"{}\")",
                iIndexInArray,
                pArray->getHandledShaderResourceName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        this->iIndexInArray = static_cast<unsigned int>(iIndexInArray);
    }

    DynamicCpuWriteShaderResourceArraySlot::~DynamicCpuWriteShaderResourceArraySlot() {
        pArray->markSlotAsNoLongerBeingUsed(this);
    }

    void DynamicCpuWriteShaderResourceArraySlot::updateData(void* pData) {
        pArray->updateSlotData(this, pData);
    }

    std::string_view DynamicCpuWriteShaderResourceArray::getHandledShaderResourceName() const {
        return sHandledShaderResourceName;
    }

    std::pair<std::recursive_mutex, DynamicCpuWriteShaderResourceArray::InternalResources>*
    DynamicCpuWriteShaderResourceArray::getInternalResources() {
        return &mtxInternalResources;
    }

    std::variant<std::unique_ptr<DynamicCpuWriteShaderResourceArray>, Error>
    DynamicCpuWriteShaderResourceArray::create(
        GpuResourceManager* pResourceManager,
        const std::string& sHandledShaderResourceName,
        size_t iElementSizeInBytes) {
        // Calculate capacity step size.
        auto result = calculateCapacityStepSize(iElementSizeInBytes);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto iCapacityStepSize = std::get<size_t>(result);

        // Self check: make sure capacity step is not zero.
        if (iCapacityStepSize == 0) [[unlikely]] {
            return Error(
                std::format("calculated capacity step size is 0 (array \"{}\")", sHandledShaderResourceName));
        }

        // Self check: make sure capacity step is even because we will use INT / INT.
        if (iCapacityStepSize % 2 != 0) [[unlikely]] {
            return Error(std::format(
                "calculated capacity step size ({}) is not even (array \"{}\")",
                iCapacityStepSize,
                sHandledShaderResourceName));
        }

        return std::unique_ptr<DynamicCpuWriteShaderResourceArray>(new DynamicCpuWriteShaderResourceArray(
            pResourceManager, sHandledShaderResourceName, iElementSizeInBytes, iCapacityStepSize));
    }

    std::string DynamicCpuWriteShaderResourceArray::formatBytesToKilobytes(size_t iSizeInBytes) {
        return std::format(
            "{:.1F} KB",
            static_cast<float>(iSizeInBytes) / 1024.0F); // NOLINT: size of one kilobyte in bytes
    }

    std::variant<size_t, Error>
    DynamicCpuWriteShaderResourceArray::calculateCapacityStepSize(size_t iElementSizeInBytes) {
        static constexpr size_t iMaxElementSizeForCapacity = 1024 * 1024 * 2; // NOLINT
        static constexpr size_t iMaxCapacityStepSize = 40;                    // NOLINT
        static constexpr size_t iMinCapacityStepSize = 2;                     // NOLINT

        // Capacity coef will be maximum at small element size and minimum at big element size.
        const auto capacityCoef = 1.0F - std::clamp(
                                             static_cast<float>(iElementSizeInBytes) /
                                                 static_cast<float>(iMaxElementSizeForCapacity),
                                             0.0F,
                                             1.0F);

        // Calculate step size.
        static_assert(iMinCapacityStepSize < iMaxElementSizeForCapacity);
        size_t iCalculatedCapacityStepSize = std::clamp(
            static_cast<size_t>(iMaxCapacityStepSize * capacityCoef),
            iMinCapacityStepSize,
            iMaxCapacityStepSize);

        if (iCalculatedCapacityStepSize % 2 != 0) {
            // Make calculated capacity even.
            // Because min/max are even:
            static_assert(iMaxCapacityStepSize % 2 == 0);
            static_assert(iMinCapacityStepSize % 2 == 0);
            // This means that we are between min/max and we just need to decide to add or to remove 1.
            if (capacityCoef > 0.5F) { // NOLINT
                iCalculatedCapacityStepSize += 1;
            } else {
                iCalculatedCapacityStepSize -= 1;
            }
        }

        return iCalculatedCapacityStepSize;
    }

    void DynamicCpuWriteShaderResourceArray::markSlotAsNoLongerBeingUsed(
        DynamicCpuWriteShaderResourceArraySlot* pSlot) {
        // Lock both self and shader resources manager because I think there might be
        // an AB-BA mutex locking issue that I described in `insert`.
        const auto pMtxShaderResources =
            pResourceManager->getRenderer()->getShaderCpuWriteResourceManager()->getResources();
        std::scoped_lock guard(mtxInternalResources.first, pMtxShaderResources->first);

        // Find the specified slot in the array of active slots.
        const auto slotIt = mtxInternalResources.second.activeSlots.find(pSlot);
        if (slotIt == mtxInternalResources.second.activeSlots.end()) [[unlikely]] {
            Logger::get().error(std::format(
                "a slot with index {} has notified the array \"{}\" about no longer being used "
                "but this slot does not exist in the set of active slots",
                pSlot->iIndexInArray,
                sHandledShaderResourceName));
            return;
        }

        // Remove from active slots.
        mtxInternalResources.second.activeSlots.erase(slotIt);

        // Add the newly unused index.
        mtxInternalResources.second.noLongerUsedArrayIndices.push(pSlot->iIndexInArray);

        const auto iCurrentSize = mtxInternalResources.second.activeSlots.size();

        // Shrink the array if needed.
        if (mtxInternalResources.second.iCapacity >= iCapacityStepSize * 2 &&
            iCurrentSize <=
                (mtxInternalResources.second.iCapacity - iCapacityStepSize - iCapacityStepSize / 2)) {
            auto optionalError = shrinkArray();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                Logger::get().error(optionalError->getFullErrorMessage());
            }
        }
    }

    void DynamicCpuWriteShaderResourceArray::updateSlotData(
        DynamicCpuWriteShaderResourceArraySlot* pSlot, void* pData) {
        // Lock both self and shader resources manager because I think there might be
        // the an AB-BA mutex locking issue that I described in `insert`.
        const auto pMtxShaderResources =
            pResourceManager->getRenderer()->getShaderCpuWriteResourceManager()->getResources();
        std::scoped_lock guard(mtxInternalResources.first, pMtxShaderResources->first);

        // Copy data.
        mtxInternalResources.second.pUploadBuffer->copyDataToElement(
            pSlot->iIndexInArray, pData, iElementSizeInBytes);
    }

    std::optional<Error> DynamicCpuWriteShaderResourceArray::createArray(size_t iCapacity) {
        std::scoped_lock guard(mtxInternalResources.first);

        // Calculate the current and the new size in bytes.
        const auto iCurrentSizeInBytes = mtxInternalResources.second.iCapacity * iElementSizeInBytes;
        const auto iNewSizeInBytes = iCapacity * iElementSizeInBytes;

        // Log the fact that we will pause the rendering.
        Logger::get().info(std::format(
            "waiting for the GPU to finish work up to this point to (re)create the GPU array "
            "\"{}\" from capacity {} ({}) to {} ({}) (current actual size: {})",
            sHandledShaderResourceName,
            mtxInternalResources.second.iCapacity,
            formatBytesToKilobytes(iCurrentSizeInBytes),
            iCapacity,
            formatBytesToKilobytes(iNewSizeInBytes),
            mtxInternalResources.second.activeSlots.size()));

        // Make sure we don't render anything and this array is not used by the GPU.
        const auto pRenderer = pResourceManager->getRenderer();
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // don't unlock render mutex until we finished updating all slots and descriptors

        // Create a new buffer.
        auto result = pResourceManager->createResourceWithCpuWriteAccess(
            std::format("\"{}\" CPU-write dynamic array", sHandledShaderResourceName),
            iElementSizeInBytes,
            iCapacity,
            true);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        mtxInternalResources.second.pUploadBuffer =
            std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            // Cast to DirectX resource.
            const auto pDirectXResource = dynamic_cast<DirectXResource*>(
                mtxInternalResources.second.pUploadBuffer->getInternalResource());
            if (pDirectXResource == nullptr) [[unlikely]] {
                return Error("expected a DirectX resource");
            }

            // Bind SRV for read access as StructuredBuffer in shaders.
            auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::SRV);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }
#endif

        // Save the new capacity.
        mtxInternalResources.second.iCapacity = iCapacity;

        // Update all active slots.
        const auto pShaderResourceManager = pRenderer->getShaderCpuWriteResourceManager();
        size_t iCurrentSlotIndex = 0;
        for (const auto& pSlot : mtxInternalResources.second.activeSlots) {
            // Set new index to slot.
            pSlot->updateIndex(iCurrentSlotIndex);
            iCurrentSlotIndex += 1;

            // Mark resource as "needs update" so that it will copy its data to the new GPU buffer.
            //
            // We use shader manager instead of telling a specific shader resource to re-copy its data to GPU
            // because of several reasons, for example because resource might already be marked as "needs
            // update" in the manager and if we tell a specific resource to re-copy its data the manager
            // will do this again.
            pShaderResourceManager->markResourceAsNeedsUpdate(pSlot->pShaderResource);
        }

        // Get global shader resource binding manager.
        const auto pGlobalBindingManager = pRenderer->getGlobalShaderResourceBindingManager();

        // Bind as global shader resource.
        auto optionalError = pGlobalBindingManager->createGlobalShaderResourceBindingSingleResource(
            sHandledShaderResourceName, mtxInternalResources.second.pUploadBuffer->getInternalResource());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> DynamicCpuWriteShaderResourceArray::expandArray() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Make sure the array is fully filled and there's no free space.
        if (mtxInternalResources.second.activeSlots.size() != mtxInternalResources.second.iCapacity)
            [[unlikely]] {
            return Error(std::format(
                "a request to expand the array \"{}\" of capacity {} with the actual size of {} "
                "was rejected, reason: expand condition is not met",
                sHandledShaderResourceName,
                mtxInternalResources.second.iCapacity,
                mtxInternalResources.second.activeSlots.size()));
        }

        // Make sure there are no unused indices.
        if (!mtxInternalResources.second.noLongerUsedArrayIndices.empty()) [[unlikely]] {
            return Error(std::format(
                "requested to expand the array \"{}\" of capacity {} while there are not used "
                "indices exist ({}) (actual size is {})",
                sHandledShaderResourceName,
                mtxInternalResources.second.iCapacity,
                mtxInternalResources.second.noLongerUsedArrayIndices.size(),
                mtxInternalResources.second.activeSlots.size()));
        }

        // Make sure our new capacity will not exceed type limit.
        constexpr auto iMaxArrayCapacity = std::numeric_limits<size_t>::max();
        if (iMaxArrayCapacity - iCapacityStepSize < mtxInternalResources.second.iCapacity) [[unlikely]] {
            return Error(std::format(
                "a request to expand the array \"{}\" of capacity {} was rejected, reason: "
                "array size will exceed the type limit of {}",
                sHandledShaderResourceName,
                mtxInternalResources.second.iCapacity,
                iMaxArrayCapacity));
        }

        // Save old array capacity to use later.
        const auto iOldArrayCapacity = mtxInternalResources.second.iCapacity;

        // Re-create the array with the new capacity.
        auto optionalError = createArray(iOldArrayCapacity + iCapacityStepSize);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Update internal values.
        mtxInternalResources.second.iNextFreeArrayIndex = iOldArrayCapacity;
        mtxInternalResources.second.noLongerUsedArrayIndices = {};

        return {};
    }

    std::optional<Error> DynamicCpuWriteShaderResourceArray::shrinkArray() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Make sure there is a space to shrink.
        const auto iMinCapacity = iCapacityStepSize * 2;
        if (mtxInternalResources.second.iCapacity < iMinCapacity) [[unlikely]] {
            return Error(std::format(
                "a request to shrink the array \"{}\" of capacity {} with the actual size of {} was "
                "rejected, reason: reached min capacity of {}",
                sHandledShaderResourceName,
                mtxInternalResources.second.iCapacity,
                mtxInternalResources.second.activeSlots.size(),
                iMinCapacity));
        }

        // Only shrink if we can erase `iCapacityStepSize` unused elements and will have some
        // free space (i.e. we will not be on the edge to expand).
        if (mtxInternalResources.second.activeSlots.size() >
            mtxInternalResources.second.iCapacity - iCapacityStepSize - iCapacityStepSize / 2) [[unlikely]] {
            return Error(std::format(
                "a request to shrink the array \"{}\" of capacity {} with the actual size of {} was "
                "rejected, reason: shrink condition is not met",
                sHandledShaderResourceName,
                mtxInternalResources.second.iCapacity,
                mtxInternalResources.second.activeSlots.size()));
        }

        const auto iNewCapacity = mtxInternalResources.second.iCapacity - iCapacityStepSize;

        // Re-create the array with the new capacity.
        auto optionalError = createArray(iNewCapacity);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Update internal values.
        mtxInternalResources.second.iNextFreeArrayIndex = iNewCapacity - iCapacityStepSize / 2;
        mtxInternalResources.second.noLongerUsedArrayIndices = {};

        return {};
    }

    DynamicCpuWriteShaderResourceArray::DynamicCpuWriteShaderResourceArray(
        GpuResourceManager* pResourceManager,
        const std::string& sHandledShaderResourceName,
        size_t iElementSizeInBytes,
        size_t iCapacityStepSize)
        : pResourceManager(pResourceManager), iCapacityStepSize(iCapacityStepSize),
          sHandledShaderResourceName(sHandledShaderResourceName), iElementSizeInBytes(iElementSizeInBytes) {}

    std::variant<std::unique_ptr<DynamicCpuWriteShaderResourceArraySlot>, Error>
    DynamicCpuWriteShaderResourceArray::insert(ShaderCpuWriteResourceBinding* pShaderResource) {
        // Make sure array's handled resource name is equal to shader resource.
        if (pShaderResource->getShaderResourceName() != sHandledShaderResourceName) [[unlikely]] {
            return Error(std::format(
                "shader resource \"{}\" requested to reserve a memory slot in the array "
                "but this array only handles shader resources with the name \"{}\"",
                pShaderResource->getShaderResourceName(),
                sHandledShaderResourceName));
        }

        // Make sure array's element size is equal to the requested one.
        if (iElementSizeInBytes != pShaderResource->getResourceDataSizeInBytes()) [[unlikely]] {
            return Error(std::format(
                "shader resource \"{}\" requested to reserve a memory slot with size {} bytes in an array "
                "but array's element size is {} bytes",
                pShaderResource->getShaderResourceName(),
                pShaderResource->getResourceDataSizeInBytes(),
                iElementSizeInBytes));
        }

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
            pResourceManager->getRenderer()->getShaderCpuWriteResourceManager()->getResources();
        std::scoped_lock guard(mtxInternalResources.first, pMtxShaderResources->first);

        // Expand the array if needed.
        if (mtxInternalResources.second.activeSlots.size() == mtxInternalResources.second.iCapacity) {
            auto optionalError = expandArray();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
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

        // Create a new slot.
        auto pNewSlot = std::unique_ptr<DynamicCpuWriteShaderResourceArraySlot>(
            new DynamicCpuWriteShaderResourceArraySlot(this, iNewIndex, pShaderResource));

        // Add new slot to an array of active slots.
        mtxInternalResources.second.activeSlots.insert(pNewSlot.get());

        return pNewSlot;
    }

    size_t DynamicCpuWriteShaderResourceArray::getSize() {
        std::scoped_lock guard(mtxInternalResources.first);
        return mtxInternalResources.second.activeSlots.size();
    }

    size_t DynamicCpuWriteShaderResourceArray::getCapacity() {
        std::scoped_lock guard(mtxInternalResources.first);
        return mtxInternalResources.second.iCapacity;
    }

    size_t DynamicCpuWriteShaderResourceArray::getSizeInBytes() {
        std::scoped_lock guard(mtxInternalResources.first);
        return mtxInternalResources.second.iCapacity * iElementSizeInBytes;
    }

    size_t DynamicCpuWriteShaderResourceArray::getElementSize() const { return iElementSizeInBytes; }

    size_t DynamicCpuWriteShaderResourceArray::getCapacityStepSize() const { return iCapacityStepSize; }

}
