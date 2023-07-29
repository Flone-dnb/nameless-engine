#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>
#include <functional>
#include <optional>

// Custom.
#include "render/general/resources/FrameResourcesManager.h"
#include "render/general/resources/UploadBuffer.h"
#include "io/Logger.h"
#include "misc/Globals.h"
#include "misc/Error.h"

namespace ne {
    class Pso;
    class GpuResource;
    class UploadBuffer;

    /** Base class for shader resources. */
    class ShaderResourceBase {
    public:
        virtual ~ShaderResourceBase() = default;

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
        [[nodiscard]] virtual std::optional<Error> updateBindingInfo(Pso* pNewPso) = 0;

        /**
         * Returns name of this resource.
         *
         * @return Resource name.
         */
        std::string getResourceName() const;

    protected:
        /**
         * Initializes the resource.
         *
         * @param sResourceName                Name of the resource we are referencing
         * (should be exactly the same as the resource name written in the shader file we are referencing).
         */
        ShaderResourceBase(const std::string& sResourceName);

    private:
        /** Name of the resource we are referencing. */
        std::string sResourceName;
    };

    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has no CPU access (cannot be updated from the CPU side, only recreated).
     */
    class ShaderResource : public ShaderResourceBase {
    public:
        virtual ~ShaderResource() override;

    protected:
        /**
         * Initializes the resource.
         *
         * @param sResourceName Name of the resource we are referencing (should be exactly the same as
         * the resource name written in the shader file we are referencing).
         * @param pResourceData Data that will be binded to this shader resource.
         */
        ShaderResource(const std::string& sResourceName, std::unique_ptr<GpuResource> pResourceData);

    private:
        /** Data binded to shader resource. */
        std::unique_ptr<GpuResource> pResourceData;
    };

    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has CPU write access available (can be updated from the CPU side).
     */
    class ShaderCpuWriteResource : public ShaderResourceBase {
        // Only manager should be able to create and update resources of this type.
        friend class ShaderCpuWriteResourceManager;

    public:
        virtual ~ShaderCpuWriteResource() override;

        /**
         * Returns original size of the resource (not padded).
         *
         * @return Size in bytes.
         */
        inline size_t getOriginalResourceSizeInBytes() const { return iOriginalResourceSizeInBytes; }

    protected:
        /**
         * Constructs not fully initialized resource.
         *
         * @param sResourceName                Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param iOriginalResourceSizeInBytes Original size of the resource (not padded).
         * @param vResourceData                Data that will be binded to this shader resource.
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         */
        ShaderCpuWriteResource(
            const std::string& sResourceName,
            size_t iOriginalResourceSizeInBytes,
            std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
                vResourceData,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

        /**
         * Copies up to date data to the GPU resource of the specified frame resource.
         *
         * @remark Should only be called when resource actually needs an update, otherwise
         * you would cause useless copy operations.
         *
         * @param iCurrentFrameResourceIndex Index of currently used frame resource.
         */
        inline void updateResource(size_t iCurrentFrameResourceIndex) {
            void* pDataToCopy = onStartedUpdatingResource();
            {
                vResourceData[iCurrentFrameResourceIndex]->copyDataToElement(
                    0, pDataToCopy, getOriginalResourceSizeInBytes());
            }
            onFinishedUpdatingResource();
        }

        /** Data binded to shader resource. */
        std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
            vResourceData;

        /**
         * Function used to update @ref vResourceData. Returns pointer to data of size
         * @ref iOriginalResourceSizeInBytes that needs to be copied into @ref vResourceData.
         */
        std::function<void*()> onStartedUpdatingResource;

        /** Function to call when finished updating (usually used for unlocking resource data mutex). */
        std::function<void()> onFinishedUpdatingResource;

        /** Original size of the resource (not padded). */
        size_t iOriginalResourceSizeInBytes = 0;
    };
} // namespace ne
