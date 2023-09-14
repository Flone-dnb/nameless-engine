#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>

// Custom.
#include "materials/resources/ShaderResource.h"
#include "render/directx/resources/DirectXResource.h"
#include "render/general/resources/UploadBuffer.h"
#include "render/general/resources/FrameResourcesManager.h"

namespace ne {
    class Pipeline;

    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has CPU write access available (can be updated from the CPU side).
     */
    class HlslShaderCpuWriteResource : public ShaderCpuWriteResource {
        // Only shader resource manager should be able to create such resources and call `updateResource`.
        friend class ShaderCpuWriteResourceManager;

    public:
        virtual ~HlslShaderCpuWriteResource() override;

        /**
         * Adds a new command to the specified command list to use this shader resource.
         *
         * @param pCommandList               Command list to add new command to.
         * @param iCurrentFrameResourceIndex Index of current frame resource.
         */
        inline void setConstantBufferView(
            const ComPtr<ID3D12GraphicsCommandList>& pCommandList, size_t iCurrentFrameResourceIndex) {
            pCommandList->SetGraphicsRootConstantBufferView(
                iRootParameterIndex,
                reinterpret_cast<DirectXResource*>(
                    vResourceData[iCurrentFrameResourceIndex]->getInternalResource())
                    ->getInternalResource()
                    ->GetGPUVirtualAddress());
        }

    protected:
        /**
         * Initializes the resource.
         *
         * @remark Used internally, for outside usage prefer to use @ref create.
         *
         * @param sResourceName                Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param pUsedPipeline                Pipeline that this resource references.
         * @param iOriginalResourceSizeInBytes Size of the resource passed to @ref create (not padded).
         * @param vResourceData                Data that will be binded to this shader resource.
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         * @param iRootParameterIndex          Index of this resource in root signature.
         */
        HlslShaderCpuWriteResource(
            const std::string& sResourceName,
            Pipeline* pUsedPipeline,
            size_t iOriginalResourceSizeInBytes,
            std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
                vResourceData,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource,
            UINT iRootParameterIndex);

        /**
         * Requires shader resource to fully (re)bind to a new/changed pipeline.
         *
         * @warning This function should not be called from outside of the class, it's only used for
         * derived implementations.
         *
         * @remark This function is generally called when pipeline that shader resource references is
         * changed or when render settings were changed and internal resources of all pipelines
         * were re-created.
         *
         * @param pNewPipeline Pipeline to bind.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindToNewPipeline(Pipeline* pNewPipeline) override;

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
         * @param pUsedPipeline              Pipeline that uses the shader/parameters we are referencing.
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
            Pipeline* pUsedPipeline,
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

        /** Index of this resource in root signature to bind this resource during the draw operation. */
        UINT iRootParameterIndex = 0;
    };
} // namespace ne
