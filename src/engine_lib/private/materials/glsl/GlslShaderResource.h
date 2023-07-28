#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>

// Custom.
#include "materials/ShaderResource.h"
#include "render/vulkan/resources/VulkanResource.h"
#include "render/general/resources/UploadBuffer.h"

namespace ne {
    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has CPU access available (can be updated from the CPU side).
     */
    class GlslShaderCpuReadWriteResource : public ShaderCpuReadWriteResource {
    public:
        virtual ~GlslShaderCpuReadWriteResource() override;

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
         * Called after the shader was changed and we want to update the binding info
         * to use this resource in the new shader without recreating the resource.
         *
         * @param pNewPso New pipeline object that is now being used instead of the old one.
         *
         * @remark Implementations will typically ask the new pipeline object about the shader resources
         * by querying root signature or descriptor layout indices and saving the index for the resource
         * with the name of this shader resource.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> updateBindingInfo(Pso* pNewPso) override;

    protected:
        /**
         * Initializes the resource.
         *
         * @param sResourceName                Name of the resource we are referencing (should be exactly
         * the same as the resource name written in the shader file we are referencing).
         * @param iOriginalResourceSizeInBytes Size of the resource passed to @ref create (not padded).
         * @param vResourceData                Data that will be binded to this shader resource.
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be
         * copied into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         */
        GlslShaderCpuReadWriteResource(
            const std::string& sResourceName,
            size_t iOriginalResourceSizeInBytes,
            std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
                vResourceData,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);
    };
} // namespace ne