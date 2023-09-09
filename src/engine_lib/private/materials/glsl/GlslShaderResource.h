#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>

// Custom.
#include "materials/resources/ShaderResource.h"
#include "render/vulkan/resources/VulkanResource.h"
#include "render/general/resources/UploadBuffer.h"
#include "render/vulkan/pipeline/VulkanPushConstantsManager.hpp"
#include "render/vulkan/resources/VulkanStorageResourceArray.h"

namespace ne {
    class Pipeline;
    class VulkanPipeline;
    class VulkanStorageResourceArraySlot;
    class VulkanRenderer;

    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has CPU write access available (can be updated from the CPU side).
     */
    class GlslShaderCpuWriteResource : public ShaderCpuWriteResource {
        // Only shader resource manager should be able to call `updateResource`.
        friend class ShaderCpuWriteResourceManager;

    public:
        virtual ~GlslShaderCpuWriteResource() override;

        /**
         * Creates a GLSL shader resource.
         *
         * @param sShaderResourceName  Name of the resource we are referencing (should be exactly the same as
         * the resource name written in the shader file we are referencing).
         * @param sResourceAdditionalInfo Additional text that we will append to created resource name
         * (used for logging).
         * @param iResourceSizeInBytes Size of the data that this resource will contain. Note that
         * this size will most likely be padded to be a multiple of 256 because of the hardware requirement
         * for shader constant buffers.
         * @param pUsedPipeline        Pipeline that uses the shader we are referencing (used to get
         * render-specific information about this resource at initialization).
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         *
         * @return Error if something went wrong, otherwise created shader resource.
         */
        static std::variant<std::unique_ptr<ShaderCpuWriteResource>, Error> create(
            const std::string& sShaderResourceName,
            const std::string& sResourceAdditionalInfo,
            size_t iResourceSizeInBytes,
            Pipeline* pUsedPipeline,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

        /**
         * Called after the shader was changed and we want to update the binding info
         * to use this resource in the new shader without recreating the resource.
         *
         * @param pNewPipeline New pipeline object that is now being used instead of the old one.
         *
         * @remark Implementations will typically ask the new pipeline object about the shader resources
         * by querying root signature or descriptor layout indices and saving the index for the resource
         * with the name of this shader resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> updateBindingInfo(Pipeline* pNewPipeline) override;

        /**
         * Copies resource index (into shader arrays) to a push constant.
         *
         * @param pPushConstantsManager      Push constants manager.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         */
        inline void copyResourceIndexToPushConstants(
            VulkanPushConstantsManager* pPushConstantsManager, size_t iCurrentFrameResourceIndex) {
            pPushConstantsManager->copyValueToPushConstant(
                iPushConstantIndex, vResourceData[iCurrentFrameResourceIndex]->getIndexIntoArray());
        }

    protected:
        /**
         * Initializes everything except for @ref vResourceData which is expected to be initialized
         * afterwards.
         *
         * @param sResourceName                Name of the resource we are referencing (should be exactly
         * the same as the resource name written in the shader file we are referencing).
         * @param iOriginalResourceSizeInBytes Size of the resource passed to @ref create (not padded).
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be
         * copied into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         */
        GlslShaderCpuWriteResource(
            const std::string& sResourceName,
            size_t iOriginalResourceSizeInBytes,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

    private:
        /**
         * Looks for a shader resource in the specified pipeline's descriptor set layout and returns
         * resource's binding index.
         *
         * @param pVulkanPipeline Pipeline to scan.
         *
         * @return Error if something went wrong, otherwise binding index of the shader resource from
         * descriptor set layout.
         */
        [[nodiscard]] std::optional<Error> updatePushConstantIndex(VulkanPipeline* pVulkanPipeline);

        /**
         * Copies up to date data to the GPU resource of the specified frame resource.
         *
         * @remark Called by shader resource manager.
         *
         * @remark Should only be called when resource actually needs an update, otherwise
         * you would cause useless copy operations.
         *
         * @param iCurrentFrameResourceIndex Index of currently used frame resource.
         */
        inline void updateResource(size_t iCurrentFrameResourceIndex) {
            void* pDataToCopy = onStartedUpdatingResource();

            vResourceData[iCurrentFrameResourceIndex]->updateData(pDataToCopy);

            onFinishedUpdatingResource();
        }

        /** Reserved space in the storage buffer that the resource uses to copy its data to. */
        std::array<
            std::unique_ptr<VulkanStorageResourceArraySlot>,
            FrameResourcesManager::getFrameResourcesCount()>
            vResourceData;

        /** Index of push constant to copy the current slot's index to (see @ref vResourceData). */
        size_t iPushConstantIndex = 0;
    };
} // namespace ne
