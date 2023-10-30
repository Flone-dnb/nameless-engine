#pragma once

// Standard.
#include <set>
#include <memory>
#include <mutex>
#include <array>
#include <unordered_map>
#include <unordered_set>

// Custom.
#include "shader/general/resources/ShaderResource.h"
#include "shader/general/resources/cpuwrite/ShaderCpuWriteResourceUniquePtr.h"
#include "render/general/resources/FrameResourcesManager.h"

namespace ne {
    class Renderer;
    class Pipeline;

    /** Stores all shader resources with CPU write access. */
    class ShaderCpuWriteResourceManager {
        // Only renderer should be allowed to create this manager.
        friend class Renderer;

    public:
        /** Groups shader CPU write resources. */
        struct Resources {
            /**
             * All shader CPU write resources.
             *
             * @remark Storing pairs of "raw pointer" - "unique pointer" to quickly find needed resources
             * when need to destroy some resource given a raw pointer.
             */
            std::unordered_map<ShaderCpuWriteResource*, std::unique_ptr<ShaderCpuWriteResource>> all;

            /** Shader CPU write resources that needs to be updated. */
            std::array<
                std::unordered_set<ShaderCpuWriteResource*>,
                FrameResourcesManager::getFrameResourcesCount()>
                vToBeUpdated;
        };

        ShaderCpuWriteResourceManager() = delete;

        ShaderCpuWriteResourceManager(const ShaderCpuWriteResourceManager&) = delete;
        ShaderCpuWriteResourceManager& operator=(const ShaderCpuWriteResourceManager&) = delete;

        /** Makes sure that no resource exists. */
        ~ShaderCpuWriteResourceManager();

        /**
         * Creates a new render-specific shader resource.
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
        std::variant<ShaderCpuWriteResourceUniquePtr, Error> createShaderCpuWriteResource(
            const std::string& sShaderResourceName,
            const std::string& sResourceAdditionalInfo,
            size_t iResourceSizeInBytes,
            const std::unordered_set<Pipeline*>& pipelinesToUse,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

        /**
         * Updates all resources marked as "needs update" and copies new (updated) data to the GPU resource
         * of the specified frame resource.
         *
         * @param iCurrentFrameResourceIndex Index of the frame resource that will be used to submit the next
         * frame.
         */
        void updateResources(size_t iCurrentFrameResourceIndex);

        /**
         * Marks resource as "needs update", this will cause resource's update callback function
         * to be later called multiple times.
         *
         * @param pResourceToDestroy Resource to mark as "needs update".
         */
        void markResourceAsNeedsUpdate(ShaderCpuWriteResource* pResourceToDestroy);

        /**
         * Destroys the specified resource because it will no longer be used.
         *
         * @param pResource Resource to destroy.
         */
        void destroyResource(ShaderCpuWriteResource* pResource);

        /**
         * Returns internal resources.
         *
         * @return Internal resources.
         */
        std::pair<std::recursive_mutex, Resources>* getResources();

    private:
        /**
         * Initializes manager.
         *
         * @param pRenderer
         */
        ShaderCpuWriteResourceManager(Renderer* pRenderer);

        /**
         * Processes resource creation.
         *
         * @param result Result of resource creation function.
         *
         * @return Result of resource creation.
         */
        std::variant<ShaderCpuWriteResourceUniquePtr, Error>
        handleResourceCreation(std::variant<std::unique_ptr<ShaderCpuWriteResource>, Error> result);

        /** Renderer that owns this manager. */
        Renderer* pRenderer = nullptr;

        /** Shader CPU write resources. */
        std::pair<std::recursive_mutex, Resources> mtxShaderCpuWriteResources;
    };
} // namespace ne
