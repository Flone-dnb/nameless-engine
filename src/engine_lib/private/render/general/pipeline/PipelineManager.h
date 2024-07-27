#pragma once

// Standard.
#include <array>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <unordered_set>

// Custom.
#include "render/general/pipeline/Pipeline.h"
#include "shader/general/ShaderMacro.h"
#include "render/general/pipeline/PipelineSharedPtr.h"
#include "shader/ComputeShaderInterface.h"
#include "render/general/pipeline/PipelineType.hpp"
#include "render/general/pipeline/PipelineConfiguration.h"
#include "render/general/pipeline/PipelineRegistry.hpp"
#include "render/general/resources/frame/FrameResourceManager.h"

// External.
#include "vulkan/vulkan_core.h"

namespace ne {
    class Renderer;
    class Material;
    class MeshNode;
    class ComputeShaderInterface;

    /**
     * RAII class that once acquired waits for the GPU to finish work up to this point, pauses the rendering,
     * releases all internal resources from all graphics pipelines, then in destructor restores them.
     *
     * @remark This can be useful when some render resource (like MSAA render target) has changed or about to
     * be changed so that we can make sure all pipelines are refreshed to use the new/changed resource.
     */
    class DelayedPipelineResourcesCreation {
    public:
        DelayedPipelineResourcesCreation() = delete;

        DelayedPipelineResourcesCreation(const DelayedPipelineResourcesCreation&) = delete;
        DelayedPipelineResourcesCreation& operator=(const DelayedPipelineResourcesCreation&) = delete;

        DelayedPipelineResourcesCreation(DelayedPipelineResourcesCreation&& other) noexcept = delete;
        DelayedPipelineResourcesCreation&
        operator=(DelayedPipelineResourcesCreation&& other) noexcept = delete;

        /**
         * Constructor.
         *
         * @param pPipelineManager Pipeline manager to use.
         */
        DelayedPipelineResourcesCreation(PipelineManager* pPipelineManager) {
            this->pPipelineManager = pPipelineManager;
            initialize();
        }

        ~DelayedPipelineResourcesCreation() { destroy(); }

    private:
        /** Does initialization logic. */
        void initialize();

        /** Does destruction logic. */
        void destroy();

        /** Do not delete (free) this pointer. Non-owning reference to pipeline manager. */
        PipelineManager* pPipelineManager = nullptr;
    };

    /**
     * Groups pointers to compute shader interfaces that were queued for execution and pipelines that they
     * use.
     *
     * @remark Only references compute shaders that use graphics queue to provide a fast access for the
     * renderer to submit those shaders (the renderer does not submit compute shaders that use
     * compute queue - they are submitted from compute shader interfaces directly).
     */
    struct QueuedForExecutionComputeShaders {
        /**
         * Stores compute pipelines and compute shader interfaces that use these pipelines
         * (one map per compute execution group).
         *
         * @remark When the renderer submits all compute shaders from this map it clears it.
         *
         * @remark Using `unordered_set` to avoid executing a compute shader multiple times.
         */
        std::array<
            std::array<
                std::unordered_map<Pipeline*, std::unordered_set<ComputeShaderInterface*>>,
                static_cast<size_t>(ComputeExecutionGroup::SIZE)>,
            static_cast<size_t>(ComputeExecutionStage::SIZE)>
            vGraphicsQueueStagesGroups;
    };

    /** Base class for managing render specific pipelines. */
    class PipelineManager {
        // Pipeline notifies the manager when a material stops referencing it.
        friend class Pipeline;

        // Releases/restores internal Pipeline's resources.
        friend class DelayedPipelineResourcesCreation;

        // Compute interfaces request compute pipelines and queue for execution.
        friend class ComputeShaderInterface;

    public:
        /**
         * Creates a new pipeline manager.
         *
         * @param pRenderer Renderer that owns this pipeline manager.
         */
        PipelineManager(Renderer* pRenderer);

        /** Checks that there are no existing pipelines left. */
        virtual ~PipelineManager();

        PipelineManager() = delete;
        PipelineManager(const PipelineManager&) = delete;
        PipelineManager& operator=(const PipelineManager&) = delete;

        /**
         * Binds the specified GPU resources (buffers, not images) to all Vulkan pipelines that use the
         * specified shader resource.
         *
         * @param vResources          Resources to bind.
         * @param sShaderResourceName Name of the shader resource (name from shader code) to bind to.
         * @param descriptorType      Type of the descriptor to bind.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindBuffersToAllVulkanPipelinesIfUsed(
            const std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()>& vResources,
            const std::string& sShaderResourceName,
            VkDescriptorType descriptorType);

        /**
         * Binds the specified GPU image (not buffer) to all Vulkan pipelines that use the specified shader
         * resource.
         *
         * @param pImageResourceToBind Image to bind.
         * @param sShaderResourceName  Name of the shader resource (name from shader code) to bind to.
         * @param descriptorType       Type of the descriptor to bind.
         * @param imageLayout          Layout of the image.
         * @param pSampler             Sampler to use for the image.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindImageToAllVulkanPipelinesIfUsed(
            GpuResource* pImageResourceToBind,
            const std::string& sShaderResourceName,
            VkDescriptorType descriptorType,
            VkImageLayout imageLayout,
            VkSampler pSampler);

        /**
         * Returns a RAII object that once acquired waits for the GPU to finish work up to this point,
         * pauses the rendering, releases all internal resources from all graphics pipelines,
         * then in destructor restores them.
         *
         * @return RAII object.
         */
        [[nodiscard]] DelayedPipelineResourcesCreation
        clearGraphicsPipelinesInternalResourcesAndDelayRestoring();

        /**
         * Look for already created pipeline that uses the specified shaders and settings and returns it,
         * otherwise creates a new pipeline.
         *
         * @remark If creating a new pipeline, loads the specified shaders from disk into the memory,
         * they will be released from the memory once the pipeline object is destroyed (not the shared
         * pointer) and no other object is using them.
         *
         * @param pPipelineConfiguration Settings that determine pipeline usage and usage details.
         * @param pMaterial              Material that requests the pipeline.
         *
         * @return Error if one or both shaders were not found in ShaderManager or if failed to generate
         * pipeline, otherwise created pipeline.
         */
        std::variant<PipelineSharedPtr, Error> getGraphicsPipelineForMaterial(
            std::unique_ptr<PipelineConfiguration> pPipelineConfiguration, Material* pMaterial);

        /**
         * Returns all compute shaders and their pipelines to be executed on the graphics queue.
         *
         * @warning Do not delete (free) returned pointers.
         *
         * @return Shaders and pipelines.
         */
        inline std::pair<std::recursive_mutex*, QueuedForExecutionComputeShaders*>
        getComputeShadersForGraphicsQueueExecution() {
            return computePipelines.getComputeShadersForGraphicsQueueExecution();
        }

        /**
         * Returns all vertex/pixel shaders and their graphics pipelines.
         *
         * @warning Do not delete (free) returned pointers.
         *
         * @return Shaders and pipelines.
         */
        inline std::pair<std::recursive_mutex, GraphicsPipelineRegistry>* getGraphicsPipelines() {
            return &mtxGraphicsPipelines;
        }

        /**
         * Returns the total number of currently existing graphics pipelines.
         *
         * @return The total number of currently existing graphics pipelines.
         */
        size_t getCurrentGraphicsPipelineCount();

        /**
         * Returns the total number of currently existing compute pipelines.
         *
         * @return The total number of currently existing compute pipelines.
         */
        size_t getCurrentComputePipelineCount();

        /**
         * Returns renderer that owns this pipeline manager.
         *
         * @return Renderer.
         */
        Renderer* getRenderer() const;

    private:
        /** Groups information about compute pipelines. */
        struct ComputePipelines {
            /** Groups mutex guarded data. */
            struct Resources {
                /** Stores pairs of "compute shader name" - "compute pipeline". */
                std::unordered_map<std::string, std::shared_ptr<Pipeline>> pipelines;

                /** Compute shader interfaces that reference pipelines from @ref pipelines. */
                QueuedForExecutionComputeShaders queuedComputeShaders;
            };

            ComputePipelines() = default;

            ComputePipelines(const ComputePipelines&) = delete;
            ComputePipelines& operator=(const ComputePipelines&) = delete;

            /**
             * Look for already created pipeline that uses the specified shader and returns it,
             * otherwise creates a new pipeline.
             *
             * @remark If creating a new pipeline, loads the specified shader from disk into the memory,
             * it will be released from the memory once the pipeline object is destroyed (not the shared
             * pointer) and no other object is using it.
             *
             * @param pPipelineManager        Pipeline manager.
             * @param pComputeShaderInterface Compute shader interface to add.
             *
             * @return Error if something went wrong, otherwise compute pipeline.
             */
            std::variant<PipelineSharedPtr, Error> getComputePipelineForShader(
                PipelineManager* pPipelineManager, ComputeShaderInterface* pComputeShaderInterface);

            /**
             * Removes the specified compute shader interface and if no other interface references the compute
             * pipeline (that the shader used) also destroys the pipeline.
             *
             * @remark If you used @ref getComputePipelineForShader to get a compute pipeline for your shader
             * you don't need to call this function as it will be automatically called by
             * `PipelineSharedPtr`'s destructor.
             *
             * @param sComputeShaderName      Name of the compute shader that compute pipeline uses.
             * @param pComputeShaderInterface Compute shader interface to remove.
             *
             * @return Error if something went wrong.
             */
            [[nodiscard]] std::optional<Error> onPipelineNoLongerUsedByComputeShaderInterface(
                const std::string& sComputeShaderName, ComputeShaderInterface* pComputeShaderInterface);

            /**
             * Adds a compute shader interface to be executed on the graphics queue according to shader's
             * execution stage and group.
             *
             * @remark Added shader will be executed only once, if you want your shader to be executed again
             * you would need to call this function again but later after a frame was submitted (if you call
             * it right now nothing will happen as it's already queued).
             *
             * @param pComputeShaderInterface Compute shader interface to queue for execution.
             *
             * @return Error if something went wrong.
             */
            [[nodiscard]] std::optional<Error>
            queueShaderExecutionOnGraphicsQueue(ComputeShaderInterface* pComputeShaderInterface);

            /**
             * Returns the total number of existing compute pipelines.
             *
             * @return Number of compute pipelines.
             */
            size_t getComputePipelineCount();

            /**
             * Returns all compute shaders and their pipelines to be executed on the graphics queue.
             *
             * @warning Do not delete (free) returned pointers.
             *
             * @return Shaders and pipelines.
             */
            inline std::pair<std::recursive_mutex*, QueuedForExecutionComputeShaders*>
            getComputeShadersForGraphicsQueueExecution() {
                return std::make_pair(&mtxResources.first, &mtxResources.second.queuedComputeShaders);
            }

        private:
            /**
             * Adds a compute shader interface to the specified map to be executed.
             *
             * @warning Expects that @ref mtxResources is locked during the function call.
             *
             * @param pipelineShaders          Map to add the new interface to.
             * @param pComputeShaderInterface  Interface to add.
             *
             * @return Error if something went wrong.
             */
            [[nodiscard]] static std::optional<Error> queueComputeShaderInterfaceForExecution(
                std::unordered_map<Pipeline*, std::unordered_set<ComputeShaderInterface*>>& pipelineShaders,
                ComputeShaderInterface* pComputeShaderInterface);

            /** Pipeline data. */
            std::pair<std::recursive_mutex, Resources> mtxResources;
        };

        /**
         * Releases internal resources (such as root signature, internal pipeline, etc.) from all
         * created graphics pipelines.
         *
         * @warning Expects that the GPU is not referencing graphics pipelines and
         * that no drawing will occur until @ref restoreInternalGraphicsPipelinesResources is called.
         *
         * @remark Causes the mutex that guards graphics pipelines to be locked until
         * @ref restoreInternalGraphicsPipelinesResources is not called and finished.
         *
         * @remark Typically used before changing something (for ex. shader configuration), so that no
         * pipeline will reference old resources, to later call
         * @ref restoreInternalGraphicsPipelinesResources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> releaseInternalGraphicsPipelinesResources();

        /**
         * Creates internal resources for all created graphics pipelines using their current
         * configuration.
         *
         * @remark Called after @ref releaseInternalGraphicsPipelinesResources to create resources that
         * will now reference changed (new) resources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> restoreInternalGraphicsPipelinesResources();

        /**
         * Assigns vertex and pixel shaders to create a render specific graphics pipeline (for usual
         * rendering).
         *
         * @param pipelines              Pipelines of specific type to look in.
         * @param sShaderNames           Shader or shaders (map key value) for target pipeline.
         * @param macrosToUse            Macros that are set (can be only vertex or combined).
         * @param pPipelineConfiguration Settings that determine pipeline usage and usage details.
         * @param pMaterial              Material that requests the pipeline.
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate pipeline,
         * otherwise created pipeline.
         */
        std::variant<PipelineSharedPtr, Error> createGraphicsPipelineForMaterial(
            std::unordered_map<std::string, ShaderPipelines>& pipelines,
            const std::string& sShaderNames,
            const std::set<ShaderMacro>& macrosToUse,
            std::unique_ptr<PipelineConfiguration> pPipelineConfiguration,
            Material* pMaterial);

        /**
         * Called from a pipeline when a material is no longer using a pipeline
         * (for ex. because changing shaders).
         *
         * @param sPipelineIdentifier Pipeline identifier.
         */
        void onPipelineNoLongerUsedByMaterial(const std::string& sPipelineIdentifier);

        /**
         * Called from a pipeline when a compute shader interface is no longer using a pipeline.
         *
         * @param sComputeShaderName      Name of the compute shader that the pipeline uses.
         * @param pComputeShaderInterface Compute shader interface that stopped using the pipeline.
         */
        void onPipelineNoLongerUsedByComputeShaderInterface(
            const std::string& sComputeShaderName, ComputeShaderInterface* pComputeShaderInterface);

        /**
         * Looks for a pipeline and returns it if found, otherwise creates and return it.
         *
         * @param pipelines                 Pipelines of specific type to look in.
         * @param sKeyToLookFor             Shader or shaders (map key value) for target pipeline.
         * @param macrosToLookFor           Macros that are set (can be only vertex or combined).
         * @param pPipelineConfiguration    Settings that determine pipeline usage and usage details.
         * @param pMaterial                 Material that requests the pipeline.
         *
         * @return Error if something went wrong, otherwise valid pipeline pointer.
         */
        std::variant<PipelineSharedPtr, Error> findOrCreatePipeline(
            std::unordered_map<std::string, ShaderPipelines>& pipelines,
            const std::string& sKeyToLookFor,
            const std::set<ShaderMacro>& macrosToLookFor,
            std::unique_ptr<PipelineConfiguration> pPipelineConfiguration,
            Material* pMaterial);

        /** Groups all graphics pipelines. */
        std::pair<std::recursive_mutex, GraphicsPipelineRegistry> mtxGraphicsPipelines;

        /** Stores all compute pipelines. */
        ComputePipelines computePipelines;

        /** Do not delete (free) this pointer. Renderer that owns this pipeline manager. */
        Renderer* pRenderer = nullptr;
    };
} // namespace ne
