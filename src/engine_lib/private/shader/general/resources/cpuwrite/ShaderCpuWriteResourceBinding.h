#pragma once

// Standard.
#include <unordered_map>
#include <optional>
#include <variant>
#include <string>
#include <functional>

// Custom.
#include "render/general/resources/frame/FrameResourceManager.h"
#include "shader/general/resources/cpuwrite/DynamicCpuWriteShaderResourceArray.h"
#include "render/general/pipeline/PipelineShaderConstantsManager.hpp"
#include "shader/general/resources/ShaderResourceBinding.h"
#include "misc/Error.h"

namespace ne {
    class Renderer;
    class Pipeline;

    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has CPU write access available (can be updated from the CPU side).
     */
    class ShaderCpuWriteResourceBinding : public ShaderResourceBindingBase {
        // Only manager should be able to create this resource and update data of this resource.
        friend class ShaderCpuWriteResourceBindingManager;

    public:
        virtual ~ShaderCpuWriteResourceBinding() override = default;

        /**
         * Copies resource index (into shader arrays) to a root/push constant.
         *
         * @param pShaderConstantsManager    Shader constants manager.
         * @param pUsedPipeline              Current pipeline.
         * @param iCurrentFrameResourceIndex Index of the current frame resource.
         */
        inline void copyResourceIndexToShaderConstants(
            PipelineShaderConstantsManager* pShaderConstantsManager,
            Pipeline* pUsedPipeline,
            size_t iCurrentFrameResourceIndex) {
            // Since pipelines won't change here (because we are inside of the `draw` function)
            // we don't need to lock the mutex here.

            // Find push constant index of this pipeline.
            const auto offsetIt = mtxUintShaderConstantOffsets.second.find(pUsedPipeline);

#if defined(DEBUG)
            if (offsetIt == mtxUintShaderConstantOffsets.second.end()) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" was requested to set its push constant "
                    "index but this shader resource does not reference the specified pipeline",
                    getShaderResourceName(),
                    mtxUintShaderConstantOffsets.second.size()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            // Copy value to root/push constants.
            pShaderConstantsManager->copyValueToShaderConstant(
                offsetIt->second, vResourceData[iCurrentFrameResourceIndex]->getIndexIntoArray());
        }

        /**
         * Returns size of the resource data.
         *
         * @return Size in bytes.
         */
        inline size_t getResourceDataSizeInBytes() const { return iResourceDataSizeInBytes; }

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
         * Creates a new shader CPU-write resource.
         *
         * @param sShaderResourceName        Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param sResourceAdditionalInfo Additional text that we will append to created resource name
         * (used for logging).
         * @param iResourceSizeInBytes       Size of the data that this resource will contain.
         * @param pipelinesToUse             Pipelines that use shader/parameters we are referencing.
         * @param onStartedUpdatingResource  Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         *
         * @return Error if something went wrong, otherwise created shader resource.
         */
        static std::variant<std::unique_ptr<ShaderCpuWriteResourceBinding>, Error> create(
            const std::string& sShaderResourceName,
            const std::string& sResourceAdditionalInfo,
            size_t iResourceSizeInBytes,
            const std::unordered_set<Pipeline*>& pipelinesToUse,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

        /**
         * Looks for root/push constants (field) named after the shader resource in the specified pipelines
         * and returns offsets of such fields.
         *
         * @param pipelines  Pipelines to scan.
         * @param sFieldName Name of the root/push constant field to look for.
         *
         * @return Error if something went wrong, otherwise pairs of "pipeline" - "offset of field".
         */
        static std::variant<std::unordered_map<Pipeline*, size_t>, Error>
        getUintShaderConstantOffsetsFromPipelines(
            const std::unordered_set<Pipeline*>& pipelines, const std::string& sFieldName);

        /**
         * Constructs a partially initialized object.
         *
         * @remark Only used internally, instead use @ref create.
         *
         * @param sShaderResourceName          Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param iResourceDataSizeInBytes     Size (in bytes) of the data that this resource will contain.
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         * @param uintShaderConstantOffsets    Offsets of root/push constants (per-pipeline) to copy an index
         * into array to.
         */
        ShaderCpuWriteResourceBinding(
            const std::string& sShaderResourceName,
            size_t iResourceDataSizeInBytes,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource,
            const std::unordered_map<Pipeline*, size_t>& uintShaderConstantOffsets);

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

        /** Offsets of root/push constants (per-pipeline) to copy an index into array to. */
        std::pair<std::recursive_mutex, std::unordered_map<Pipeline*, size_t>> mtxUintShaderConstantOffsets;

        /**
         * Stores data for shaders to use (per frame resource). Index (from array start)
         * will be copied to push constant so that shaders can index into the array and access the data.
         */
        std::array<
            std::unique_ptr<DynamicCpuWriteShaderResourceArraySlot>,
            FrameResourceManager::getFrameResourceCount()>
            vResourceData;

    private:
        /**
         * Copies up to date data to the GPU resource of the specified frame resource.
         *
         * @remark Called by shader resource manager.
         *
         * @remark Should only be called when resource actually needs an update, otherwise
         * you would cause a useless copy operations.
         *
         * @param iCurrentFrameResourceIndex Index of currently used frame resource.
         */
        inline void updateResource(size_t iCurrentFrameResourceIndex) {
            void* pDataToCopy = onStartedUpdatingResource();

            vResourceData[iCurrentFrameResourceIndex]->updateData(pDataToCopy);

            onFinishedUpdatingResource();
        }

        /**
         * Function used to update resource data. Returns pointer to data of size
         * @ref iResourceDataSizeInBytes that needs to be copied into resource data storage (GPU
         * resource).
         */
        const std::function<void*()> onStartedUpdatingResource;

        /** Function to call when finished updating (usually used for unlocking resource data mutex). */
        const std::function<void()> onFinishedUpdatingResource;

        /** Size (in bytes) of the data that the resource contains. */
        const size_t iResourceDataSizeInBytes = 0;
    };
}
