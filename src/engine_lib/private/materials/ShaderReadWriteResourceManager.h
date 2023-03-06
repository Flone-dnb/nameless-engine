#pragma once

// Standard.
#include <set>
#include <memory>
#include <mutex>

// Custom.
#include "materials/ShaderResource.h"
#include "materials/ShaderReadWriteResourceUniquePtr.h"

namespace ne {
    class Renderer;

    /** Stores all shader resources with CPU read/write access. */
    class ShaderCpuReadWriteResourceManager {
        // Only renderer should be allowed to create this manager.
        friend class Renderer;

    public:
        /** Groups shader read/write resources. */
        struct Resources {
            /** All created shader CPU read/write resources. */
            std::vector<std::unique_ptr<ShaderCpuReadWriteResource>> vAll;

            /** Shader CPU read/write resources that needs to be updated. */
            std::set<ShaderCpuReadWriteResource*> toBeUpdated;
        };

        ShaderCpuReadWriteResourceManager(const ShaderCpuReadWriteResourceManager&) = delete;
        ShaderCpuReadWriteResourceManager& operator=(const ShaderCpuReadWriteResourceManager&) = delete;

        /**
         * Creates a new render-specific shader resource.
         *
         * @param sShaderResourceName      Name of the resource we are referencing (should be exactly the same
         * as the resource name written in the shader file we are referencing).
         * @param sResourceAdditionalInfo  Additional text that we will append to created resource name
         * (used for logging).
         * @param iResourceSizeInBytes     Size of the data that this resource will contain. Note that
         * this size will most likely be padded to be a multiple of 256 because of the hardware requirement
         * for shader constant buffers.
         * @param pUsedPso                 PSO that uses the shader we are referencing (used to get
         * render-specific information about this resource at initialization).
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         *
         * @return Error if something went wrong, otherwise created shader resource.
         */
        std::variant<ShaderCpuReadWriteResourceUniquePtr, Error> createShaderCpuReadWriteResource(
            const std::string& sShaderResourceName,
            const std::string& sResourceAdditionalInfo,
            size_t iResourceSizeInBytes,
            Pso* pUsedPso,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

        /**
         * Updates all resources that marked as "needs update".
         *
         * @param iCurrentFrameResourceIndex Index of current frame resource.
         */
        void updateResources(size_t iCurrentFrameResourceIndex);

        /**
         * Marks resource as "needs update", this will cause resource's update callback function
         * to be later called multiple times.
         *
         * @param pResourceToDestroy Resource to mark as "needs update".
         */
        void markResourceAsNeedsUpdate(ShaderCpuReadWriteResource* pResourceToDestroy);

        /**
         * Destroys the specified resource because it will no longer be used.
         *
         * @param pResource Resource to destroy.
         */
        void destroyResource(ShaderCpuReadWriteResource* pResource);

        /**
         * Returns internal resources.
         *
         * @return Internal resources.
         */
        std::pair<std::mutex, Resources>* getResources();

    private:
        ShaderCpuReadWriteResourceManager() = delete;

        /**
         * Initializes manager.
         *
         * @param pRenderer
         */
        ShaderCpuReadWriteResourceManager(Renderer* pRenderer);

        /**
         * Processes resource creation.
         *
         * @param result Result of resource creation function.
         *
         * @return Result of resource creation.
         */
        std::variant<ShaderCpuReadWriteResourceUniquePtr, Error>
        handleResourceCreation(std::variant<std::unique_ptr<ShaderCpuReadWriteResource>, Error> result);

        /** Renderer that owns this manager. */
        Renderer* pRenderer = nullptr;

        /** Shader read/write resources. */
        std::pair<std::mutex, Resources> mtxShaderCpuReadWriteResources;

        /** Name of the category used for logging. */
        static inline const auto sShaderCpuReadWriteResourceManagerLogCategory =
            "Shader CPU Read/Write Resource Manager";
    };
} // namespace ne
