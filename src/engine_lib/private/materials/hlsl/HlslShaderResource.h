#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>

// Custom.
#include "materials/ShaderResource.h"
#include "render/directx/resources/DirectXResource.h"
#include "render/general/resources/UploadBuffer.h"

namespace ne {
    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has CPU access available (can be updated from the CPU side).
     */
    class HlslShaderCpuReadWriteResource : public ShaderCpuReadWriteResource {
    public:
        virtual ~HlslShaderCpuReadWriteResource() override;

        /**
         * Creates a new render-specific shader resource.
         *
         * @param sShaderResourceName  Name of the resource we are referencing (should be exactly the same as
         * the resource name written in the shader file we are referencing).
         * @param sResourceAdditionalInfo Additional text that we will append to created resource name
         * (used for logging).
         * @param iResourceSizeInBytes Size of the data that this resource will contain. Note that
         * this size will most likely be padded to be a multiple of 256 because of the hardware requirement
         * for shader constant buffers.
         * @param pUsedPso             PSO that uses the shader we are referencing (used to get
         * render-specific information about this resource at initialization).
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         *
         * @return Error if something went wrong, otherwise created shader resource.
         */
        static std::variant<std::unique_ptr<ShaderCpuReadWriteResource>, Error> create(
            const std::string& sShaderResourceName,
            const std::string& sResourceAdditionalInfo,
            size_t iResourceSizeInBytes,
            Pso* pUsedPso,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

        /**
         * Adds a new command to the specified command list to use this shader resource.
         *
         * @param pCommandList               Command list to add new command to.
         * @param iCurrentFrameResourceIndex Index of current frame resource.
         */
        inline void setToPipeline(
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
         * @param sResourceName                Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param iOriginalResourceSizeInBytes Size of the resource passed to @ref create (not padded).
         * @param vResourceData                Data that will be binded to this shader resource.
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         * @param iRootParameterIndex          Index of this resource in root signature.
         */
        HlslShaderCpuReadWriteResource(
            const std::string& sResourceName,
            size_t iOriginalResourceSizeInBytes,
            std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
                vResourceData,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource,
            UINT iRootParameterIndex);

    private:
        /** Index of this resource in root signature. */
        UINT iRootParameterIndex = 0;
    };
} // namespace ne