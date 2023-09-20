#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>
#include <mutex>
#include <unordered_map>

// Custom.
#include "materials/resources/ShaderResource.h"
#include "render/vulkan/resources/VulkanResource.h"
#include "render/general/resources/UploadBuffer.h"
#include "render/vulkan/pipeline/VulkanPushConstantsManager.hpp"
#include "render/vulkan/resources/VulkanStorageResourceArray.h"
#include "render/general/resources/FrameResourcesManager.h"

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
        // Only shader resource manager should be able to create such resources and call `updateResource`.
        friend class ShaderCpuWriteResourceManager;

    public:
        virtual ~GlslShaderCpuWriteResource() override = default;

        /**
         * Copies resource index (into shader arrays) to a push constant.
         *
         * @warning Expects that this shader resource uses only 1 pipeline. Generally used for
         * material's resources because material can only reference 1 pipeline unlike mesh nodes.
         *
         * @param pPushConstantsManager      Push constants manager.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         */
        inline void copyResourceIndexOfOnlyPipelineToPushConstants(
            VulkanPushConstantsManager* pPushConstantsManager, size_t iCurrentFrameResourceIndex) {
            // Since pipelines won't change here (because we are inside of the `draw` function)
            // we don't need to lock the mutex here.

#if defined(DEBUG)
            // Self check: make sure there is indeed just 1 pipeline.
            if (mtxPushConstantIndices.second.size() != 1) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" was requested to set its push constant "
                    "index of the only used pipeline but this shader resource references "
                    "{} pipeline(s)",
                    getResourceName(),
                    mtxPushConstantIndices.second.size()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            // Copy value to push constants.
            pPushConstantsManager->copyValueToPushConstant(
                mtxPushConstantIndices.second.begin()->second,
                vResourceData[iCurrentFrameResourceIndex]->getIndexIntoArray());
        }

        /**
         * Copies resource index (into shader arrays) to a push constant.
         *
         * @param pPushConstantsManager      Push constants manager.
         * @param pUsedPipeline              Current pipeline.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         */
        inline void copyResourceIndexOfPipelineToPushConstants(
            VulkanPushConstantsManager* pPushConstantsManager,
            VulkanPipeline* pUsedPipeline,
            size_t iCurrentFrameResourceIndex) {
            // Since pipelines won't change here (because we are inside of the `draw` function)
            // we don't need to lock the mutex here.

            // Find push constant index of this pipeline.
            const auto it = mtxPushConstantIndices.second.find(pUsedPipeline);
            if (it == mtxPushConstantIndices.second.end()) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" was requested to set its push constant "
                    "index but this shader resource does not reference the specified pipeline",
                    getResourceName(),
                    mtxPushConstantIndices.second.size()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Copy value to push constants.
            pPushConstantsManager->copyValueToPushConstant(
                it->second, vResourceData[iCurrentFrameResourceIndex]->getIndexIntoArray());
        }

        /**
         * Called to make the resource to discard currently used pipelines and bind/reference
         * other pipelines.
         *
         * @warning Expects that the caller is using some mutex to protect this shader resource
         * from being used in the `draw` function while this function is not finished
         * (i.e. make sure the CPU will not queue a new frame while this function is not finished).
         *
         * @remark For example, for this function can be called from a mesh node that changed
         * its geometry and thus added/removed some material slots, or if some material that mesh node
         * is using changed its pipeline.
         *
         * @param pipelinesToUse Pipelines to use instead of the current ones.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        changeUsedPipelines(const std::unordered_set<Pipeline*>& pipelinesToUse) override;

    protected:
        /**
         * Initializes everything except for @ref vResourceData which is expected to be initialized
         * afterwards.
         *
         * @remark Used internally, for outside usage prefer to use @ref create.
         *
         * @param sResourceName                Name of the resource we are referencing (should be exactly
         * the same as the resource name written in the shader file we are referencing).
         * @param iOriginalResourceSizeInBytes Size of the resource passed to @ref create (not padded).
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be
         * copied into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         * @param pushConstantIndices          Indices of push constants (per-pipeline) to copy
         * index into @ref vResourceData.
         */
        GlslShaderCpuWriteResource(
            const std::string& sResourceName,
            size_t iOriginalResourceSizeInBytes,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource,
            std::unordered_map<VulkanPipeline*, size_t> pushConstantIndices);

        /**
         * Called from pipeline manager to notify that all pipelines released their internal resources
         * and now restored them so their internal resources (for example push constants) might
         * be different now and shader resource now needs to check that everything that it needs
         * is still there and possibly re-bind to pipeline's descriptors since these might have
         * been also re-created.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> onAfterAllPipelinesRefreshedResources() override;

    private:
        /**
         * Creates a GLSL shader resource with CPU write access.
         *
         * @param sShaderResourceName        Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param sResourceAdditionalInfo    Additional text that we will append to created resource name
         * (used for logging).
         * @param iResourceSizeInBytes       Size of the data that this resource will contain. Note that
         * this size will most likely be padded to be a multiple of 256 because of the hardware requirement
         * for shader constant buffers.
         * @param pipelinesToUse             Pipelines that use shader/parameters we are referencing.
         * @param onStartedUpdatingResource  Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         *
         * @return Error if something went wrong, otherwise created shader resource.
         */
        static std::variant<std::unique_ptr<ShaderCpuWriteResource>, Error> create(
            const std::string& sShaderResourceName,
            const std::string& sResourceAdditionalInfo,
            size_t iResourceSizeInBytes,
            std::unordered_set<Pipeline*> pipelinesToUse,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

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

        /**
         * Index of push constant (per-pipeline) to copy the current slot's index to (see @ref
         * vResourceData).
         */
        std::pair<std::recursive_mutex, std::unordered_map<VulkanPipeline*, size_t>> mtxPushConstantIndices;
    };
} // namespace ne
