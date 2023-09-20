#pragma once

// Standard.
#include <string>
#include <memory>
#include <unordered_set>
#include <variant>

// Custom.
#include "materials/resources/ShaderResource.h"
#include "render/directx/resources/DirectXResource.h"
#include "render/general/resources/UploadBuffer.h"
#include "render/general/resources/FrameResourcesManager.h"

namespace ne {
    class Pipeline;
    class DirectXPso;

    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has CPU write access available (can be updated from the CPU side).
     */
    class HlslShaderCpuWriteResource : public ShaderCpuWriteResource {
        // Only shader resource manager should be able to create such resources and call `updateResource`.
        friend class ShaderCpuWriteResourceManager;

    public:
        virtual ~HlslShaderCpuWriteResource() override = default;

        /**
         * Adds a new command to the specified command list to use this shader resource.
         *
         * @warning Expects that this shader resource uses only 1 pipeline. Generally used for
         * material's resources because material can only reference 1 pipeline unlike mesh nodes.
         *
         * @param pCommandList               Command list to add new command to.
         * @param iCurrentFrameResourceIndex Index of current frame resource.
         */
        inline void setConstantBufferViewOfOnlyPipeline(
            const ComPtr<ID3D12GraphicsCommandList>& pCommandList, size_t iCurrentFrameResourceIndex) {
            // Since pipelines won't change here (because we are inside of the `draw` function)
            // we don't need to lock the mutex here.

#if defined(DEBUG)
            // Self check: make sure there is indeed just 1 pipeline.
            if (mtxRootParameterIndices.second.size() != 1) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" was requested to set its constant buffer "
                    "view of the only used pipeline but this shader resource references "
                    "{} pipeline(s)",
                    getResourceName(),
                    mtxRootParameterIndices.second.size()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            // Set view.
            pCommandList->SetGraphicsRootConstantBufferView(
                mtxRootParameterIndices.second.begin()->second,
                reinterpret_cast<DirectXResource*>(
                    vResourceData[iCurrentFrameResourceIndex]->getInternalResource())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());
        }

        /**
         * Adds a new command to the specified command list to use this shader resource.
         *
         * @param pCommandList               Command list to add new command to.
         * @param pUsedPipeline              Current pipeline.
         * @param iCurrentFrameResourceIndex Index of current frame resource.
         */
        inline void setConstantBufferViewOfPipeline(
            const ComPtr<ID3D12GraphicsCommandList>& pCommandList,
            DirectXPso* pUsedPipeline,
            size_t iCurrentFrameResourceIndex) {
            // Since pipelines won't change here (because we are inside of the `draw` function)
            // we don't need to lock the mutex here.

            // Find root parameter index of this pipeline.
            const auto it = mtxRootParameterIndices.second.find(pUsedPipeline);
            if (it == mtxRootParameterIndices.second.end()) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" was requested to set its constant buffer view "
                    "but this shader resource does not reference the specified pipeline",
                    getResourceName(),
                    mtxRootParameterIndices.second.size()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Set view.
            pCommandList->SetGraphicsRootConstantBufferView(
                it->second,
                reinterpret_cast<DirectXResource*>(
                    vResourceData[iCurrentFrameResourceIndex]->getInternalResource())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());
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
         * Initializes the resource.
         *
         * @remark Used internally, for outside usage prefer to use @ref create.
         *
         * @param sResourceName                Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param iOriginalResourceSizeInBytes Size of the resource passed to @ref create (not padded).
         * @param vResourceData                Data that will be binded to this shader resource.
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         * @param rootParameterIndices         Indices of this resource in root signature.
         */
        HlslShaderCpuWriteResource(
            const std::string& sResourceName,
            size_t iOriginalResourceSizeInBytes,
            std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
                vResourceData,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource,
            std::unordered_map<DirectXPso*, UINT> rootParameterIndices);

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
         * Creates a new HLSL shader resource.
         *
         * @param sShaderResourceName        Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param sResourceAdditionalInfo Additional text that we will append to created resource name
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
         * @remark Should only be called when resource actually needs an update, otherwise
         * you would cause useless copy operations.
         *
         * @remark Called by shader resource manager.
         *
         * @param iCurrentFrameResourceIndex Index of currently used frame resource.
         */
        inline void updateResource(size_t iCurrentFrameResourceIndex) {
            void* pDataToCopy = onStartedUpdatingResource();

            vResourceData[iCurrentFrameResourceIndex]->copyDataToElement(
                0, pDataToCopy, getOriginalResourceSizeInBytes());

            onFinishedUpdatingResource();
        }

        /** Data binded to shader resource. */
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData;

        /** Indices of this resource in root signature to bind this resource during the draw operation. */
        std::pair<std::recursive_mutex, std::unordered_map<DirectXPso*, UINT>> mtxRootParameterIndices;
    };
} // namespace ne
