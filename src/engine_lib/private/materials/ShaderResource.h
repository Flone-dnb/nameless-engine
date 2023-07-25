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
    class ShaderResource {
    public:
        virtual ~ShaderResource() = default;

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

    protected:
        /**
         * Initializes the resource.
         *
         * @param sResourceName Name of the resource we are referencing (should be exactly the same as
         * the resource name written in the shader file we are referencing).
         */
        ShaderResource(const std::string& sResourceName);

        /**
         * Returns name of this resource.
         *
         * @return Resource name.
         */
        std::string getResourceName() const;

    private:
        /** Name of the resource we are referencing. */
        std::string sResourceName;
    };

    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has no CPU access (cannot be updated from the CPU side, only recreated).
     */
    class ShaderCpuReadOnlyResource : public ShaderResource {
    public:
        virtual ~ShaderCpuReadOnlyResource() override;

    protected:
        /**
         * Initializes the resource.
         *
         * @param sResourceName Name of the resource we are referencing (should be exactly the same as
         * the resource name written in the shader file we are referencing).
         * @param pResourceData Data that will be binded to this shader resource.
         */
        ShaderCpuReadOnlyResource(
            const std::string& sResourceName, std::unique_ptr<GpuResource> pResourceData);

    private:
        /** Data binded to shader resource. */
        std::unique_ptr<GpuResource> pResourceData;
    };

    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has CPU access available (can be updated from the CPU side).
     */
    class ShaderCpuReadWriteResource : public ShaderResource {
        // Only manager should be able to create and update resources of this type.
        friend class ShaderCpuReadWriteResourceManager;

    public:
        virtual ~ShaderCpuReadWriteResource() override;

    protected:
        /**
         * Marks resource as "needs update", causes resource's update callback
         * to be later called multiple times.
         */
        void markAsNeedsUpdate();

        /**
         * Updates shader resource with up to date data.
         *
         * @remark Should only be called when resource actually needs an update.
         *
         * @param iCurrentFrameResourceIndex Index of currently used frame resource.
         *
         * @return `true` if this resource no longer needs to be updated (for now), `false`
         * if this resource also needs to be updated for next frame resource.
         */
        inline bool updateResource(size_t iCurrentFrameResourceIndex) {
#if defined(DEBUG)
            // Self check:
            if (iFrameResourceCountToUpdate.load() == 0) [[unlikely]] {
                Logger::get().error(fmt::format(
                    "{} shader read write resource \"{}\" was updated while no update was "
                    "needed, update flag will now have incorrect state",
                    Globals::getDebugOnlyLoggingSubCategoryName(),
                    getResourceName()));
            }
#endif

            void* pDataToCopy = onStartedUpdatingResource();
            {
                vResourceData[iCurrentFrameResourceIndex]->copyDataToElement(
                    0, pDataToCopy, iOriginalResourceSizeInBytes);
            }
            onFinishedUpdatingResource();

            return (iFrameResourceCountToUpdate.fetch_sub(1) - 1) == 0;
        }

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
        ShaderCpuReadWriteResource(
            const std::string& sResourceName,
            size_t iOriginalResourceSizeInBytes,
            std::array<std::unique_ptr<UploadBuffer>, FrameResourcesManager::getFrameResourcesCount()>
                vResourceData,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

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

        /** Defines how much elements in @ref vResourceData need to be updated. */
        std::atomic<unsigned int> iFrameResourceCountToUpdate{
            FrameResourcesManager::getFrameResourcesCount()};

        /** Original size of the resource (not padded). */
        size_t iOriginalResourceSizeInBytes = 0;
    };

    /**
     * References a shader array resource (that is written in a shader file)
     * that has no CPU access (cannot be updated from the CPU side, only recreated).
     */
    class ShaderCpuReadOnlyArrayResource {
    public:
        virtual ~ShaderCpuReadOnlyArrayResource() = default;

    protected:
        ShaderCpuReadOnlyArrayResource() = default;
    };
} // namespace ne
