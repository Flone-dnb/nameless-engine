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

namespace ne {
    class Renderer;
    class Material;
    class MeshNode;
    class ComputeShaderInterface;

    /**
     * Small wrapper class for `std::shared_ptr<Pipeline>` to do some extra work
     * when started/stopped referencing a pipeline.
     */
    class PipelineSharedPtr {
    public:
        /**
         * Constructs a new pointer for a material that uses a pipeline.
         *
         * @param pPipeline                 Pipeline to store.
         * @param pMaterialThatUsesPipeline Material that stores this pointer.
         */
        explicit PipelineSharedPtr(std::shared_ptr<Pipeline> pPipeline, Material* pMaterialThatUsesPipeline) {
            initialize(std::move(pPipeline), pMaterialThatUsesPipeline);
        }

        /**
         * Constructs a new pointer for a compute shader interface that uses a pipeline.
         *
         * @param pPipeline                      Pipeline to store.
         * @param pComputeShaderThatUsesPipeline Compute interface that stores this pointer.
         */
        explicit PipelineSharedPtr(
            std::shared_ptr<Pipeline> pPipeline, ComputeShaderInterface* pComputeShaderThatUsesPipeline) {
            initialize(std::move(pPipeline), pComputeShaderThatUsesPipeline);
        }

        /** Leaves the internal pointers initialized as `nullptr`. */
        PipelineSharedPtr() = default;

        ~PipelineSharedPtr() { clearPointerAndNotifyPipeline(); }

        PipelineSharedPtr(const PipelineSharedPtr&) = delete;
        PipelineSharedPtr& operator=(const PipelineSharedPtr&) = delete;

        /**
         * Move constructor.
         *
         * @param other other object.
         */
        PipelineSharedPtr(PipelineSharedPtr&& other) noexcept { *this = std::move(other); }

        /**
         * Move assignment.
         *
         * @param other other object.
         *
         * @return Result of move assignment.
         */
        PipelineSharedPtr& operator=(PipelineSharedPtr&& other) noexcept {
            if (this != &other) {
                if (other.pPipeline != nullptr) {
                    pPipeline = std::move(other.pPipeline);
                    other.pPipeline = nullptr;
                }

                pMaterialThatUsesPipeline = other.pMaterialThatUsesPipeline;
                pComputeShaderThatUsesPipeline = other.pComputeShaderThatUsesPipeline;

                other.pMaterialThatUsesPipeline = nullptr;
                other.pComputeShaderThatUsesPipeline = nullptr;
            }

            return *this;
        }

        /**
         * Tells whether the internal pipeline was set or not.
         *
         * @return Whether the internal pipeline was set or not.
         */
        bool isInitialized() const { return pPipeline != nullptr; }

        /** Clears the pointer (sets to `nullptr`). */
        void clear() { clearPointerAndNotifyPipeline(); }

        /**
         * Changes stored pipeline to another one.
         *
         * @param pPipeline                 Pipeline to use.
         * @param pMaterialThatUsesPipeline Material that stores this pointer.
         */
        void set(std::shared_ptr<Pipeline> pPipeline, Material* pMaterialThatUsesPipeline) {
            clearPointerAndNotifyPipeline();
            initialize(std::move(pPipeline), pMaterialThatUsesPipeline);
        }

        /**
         * Returns pointer to underlying pipeline.
         *
         * @warning Do not delete returned pointer.
         *
         * @return Raw pointer to the underlying pipeline.
         */
        Pipeline* getPipeline() const { return pPipeline.get(); }

        /**
         * Access operator.
         *
         * @return Raw pointer to the underlying pipeline.
         */
        Pipeline* operator->() const { return pPipeline.get(); }

    private:
        /** Clears stored shared pointer and notifies the Pipeline that we no longer reference it. */
        void clearPointerAndNotifyPipeline() {
            if (pPipeline == nullptr) {
                // This object was moved.
                return;
            }

            auto pPipelineRaw = pPipeline.get();
            pPipeline = nullptr; // clear shared pointer before calling notify function

            if (pMaterialThatUsesPipeline != nullptr) {
                pPipelineRaw->onMaterialNoLongerUsingPipeline(pMaterialThatUsesPipeline);
            } else if (pComputeShaderThatUsesPipeline != nullptr) {
                pPipelineRaw->onComputeShaderNoLongerUsingPipeline(pComputeShaderThatUsesPipeline);
            } else [[unlikely]] {
                Error error(std::format(
                    "pipeline shared pointer to pipeline \"{}\" is being destroyed but "
                    "pointers to material and compute interface are `nullptr` - unable to notify manager",
                    pPipelineRaw->getPipelineIdentifier()));
            }
        }

        /**
         * Initializes internal state.
         *
         * @param pPipeline                 Pipeline to store.
         * @param pMaterialThatUsesPipeline Material that stores this pointer.
         */
        void initialize(std::shared_ptr<Pipeline> pPipeline, Material* pMaterialThatUsesPipeline) {
            this->pPipeline = std::move(pPipeline);
            this->pMaterialThatUsesPipeline = pMaterialThatUsesPipeline;

            // Notify pipeline.
            this->pPipeline->onMaterialUsingPipeline(pMaterialThatUsesPipeline);
        }

        /**
         * Initializes internal state.
         *
         * @param pPipeline                      Pipeline to store.
         * @param pComputeShaderThatUsesPipeline Compute interface that stores this pointer.
         */
        void initialize(
            std::shared_ptr<Pipeline> pPipeline, ComputeShaderInterface* pComputeShaderThatUsesPipeline) {
            this->pPipeline = std::move(pPipeline);
            this->pComputeShaderThatUsesPipeline = pComputeShaderThatUsesPipeline;

            // Notify pipeline.
            this->pPipeline->onComputeShaderUsingPipeline(pComputeShaderThatUsesPipeline);
        }

        /** Internally stored pipeline */
        std::shared_ptr<Pipeline> pPipeline = nullptr;

        /**
         * Material that stores this pointer.
         *
         * @remark If `nullptr` then @ref pComputeShaderThatUsesPipeline is valid.
         */
        Material* pMaterialThatUsesPipeline = nullptr;

        /**
         * Compute shader interface that stores this pointer.
         *
         * @remark If `nullptr` then @ref pMaterialThatUsesPipeline is valid.
         */
        ComputeShaderInterface* pComputeShaderThatUsesPipeline = nullptr;
    };

    /**
     * RAII class that once acquired waits for the GPU to finish work up to this point, pauses the rendering,
     * releases all internal resources from all graphics pipelines, then in destructor restores them.
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

    /** Base class for managing render specific pipelines. */
    class PipelineManager {
        // Pipeline notifies the manager when a material stops referencing it.
        friend class Pipeline;

        // Releases/restores internal Pipeline's resources.
        friend class DelayedPipelineResourcesCreation;

        // Compute interfaces request compute pipelines and queue for execution.
        friend class ComputeShaderInterface;

    public:
        /** Groups information about pipelines that use the same shaders. */
        struct ShaderPipelines {
            /**
             * Map of pairs "material defined macros (combined both vertex and pixel macros)" and
             * "pipelines that were created from the same shader to use these different macros".
             *
             * @remark Since shader macros have prefixes that define which shader stage they are
             * valid for we won't have problems with same macros on different shader stages
             * (because of combining and storing all macros in just one `std::set`).
             */
            std::unordered_map<std::set<ShaderMacro>, std::shared_ptr<Pipeline>, ShaderMacroSetHash>
                shaderPipelines;
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
             * Stores compute pipelines and compute shader interfaces that use these pipelines and were queued
             * for execution before a frame is rendered.
             *
             * @remark Compute shaders will be retrieved from pipeline.
             *
             * @remark When the renderer submits all compute shaders from this map it clears it.
             *
             * @remark Using `unordered_set` to avoid executing a compute shader multiple times.
             */
            std::unordered_map<Pipeline*, std::unordered_set<ComputeShaderInterface*>>
                graphicsQueuePreFrameShaders;

            /**
             * Stores compute pipelines and compute shader interfaces that use these pipelines and were queued
             * for execution after a frame is rendered.
             *
             * @remark Compute shaders will be retrieved from pipeline.
             *
             * @remark When the renderer submits all compute shaders from this map it clears it.
             *
             * @remark Using `unordered_set` to avoid executing a compute shader multiple times.
             */
            std::unordered_map<Pipeline*, std::unordered_set<ComputeShaderInterface*>>
                graphicsQueuePostFrameShaders;
        };

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
         * @param sVertexShaderName    Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName     Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending    Whether the pixels of the mesh that uses this pipeline should blend
         * with existing pixels on back buffer or not (for transparency).
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
         * @param additionalPixelShaderMacros  Additional macros to enable for pixel shader configuration.
         * @param pMaterial            Material that requests the pipeline.
         *
         * @return Error if one or both shaders were not found in ShaderManager or if failed to generate
         * pipeline, otherwise created pipeline.
         */
        std::variant<PipelineSharedPtr, Error> getGraphicsPipelineForMaterial(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            const std::set<ShaderMacro>& additionalPixelShaderMacros,
            Material* pMaterial);

        /**
         * Returns an array currently existing graphics pipelines that contains a map per pipeline type
         * where each map contains a pipeline identifier string (vertex/pixel shader name combined)
         * and pipelines that use these shaders. Must be used with mutex.
         *
         * @return Array of currently existing graphics pipelines.
         */
        std::array<
            std::pair<std::recursive_mutex, std::unordered_map<std::string, ShaderPipelines>>,
            static_cast<size_t>(PipelineType::SIZE)>*
        getGraphicsPipelines();

        /**
         * Returns all compute shaders and their pipelines to be executed on the graphics queue.
         *
         * @warning Do not delete (free) returned pointers.
         *
         * @return Shaders and pipelines.
         */
        inline std::pair<std::mutex*, QueuedForExecutionComputeShaders*>
        getComputeShadersForGraphicsQueueExecution() {
            return computePipelines.getComputeShadersForGraphicsQueueExecution();
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
             * Adds a compute shader interface to be executed on the graphics queue before a frame is
             * rendered.
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
            queueShaderExecutionOnGraphicsQueuePreFrame(ComputeShaderInterface* pComputeShaderInterface);

            /**
             * Adds a compute shader interface to be executed on the graphics queue after a frame is
             * rendered.
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
            queueShaderExecutionOnGraphicsQueuePostFrame(ComputeShaderInterface* pComputeShaderInterface);

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
            inline std::pair<std::mutex*, QueuedForExecutionComputeShaders*>
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
            [[nodiscard]] std::optional<Error> queueComputeShaderInterfaceForExecution(
                std::unordered_map<Pipeline*, std::unordered_set<ComputeShaderInterface*>>& pipelineShaders,
                ComputeShaderInterface* pComputeShaderInterface);

            /** Pipeline data. */
            std::pair<std::mutex, Resources> mtxResources;
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
         * @param sVertexShaderName    Name of the compiled vertex shader (see
         * ShaderManager::compileShaders).
         * @param sPixelShaderName     Name of the compiled pixel shader (see
         * ShaderManager::compileShaders).
         * @param bUsePixelBlending    Whether the pixels of the mesh that uses this pipeline should blend
         * with existing pixels on back buffer or not (for transparency).
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader
         * configuration.
         * @param additionalPixelShaderMacros  Additional macros to enable for pixel shader configuration.
         * @param pMaterial            Material that requests the pipeline.
         *
         * @return Error if one or both were not found in ShaderManager or if failed to generate pipeline,
         * otherwise created pipeline.
         */
        std::variant<PipelineSharedPtr, Error> createGraphicsPipelineForMaterial(
            const std::string& sVertexShaderName,
            const std::string& sPixelShaderName,
            bool bUsePixelBlending,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            const std::set<ShaderMacro>& additionalPixelShaderMacros,
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
         * Array that stores a map of pipelines per pipeline type.
         * Map stores pairs of "combination of vertex/pixel(fragment) shader names" and
         * "pipelines that use these shaders".
         */
        std::array<
            std::pair<std::recursive_mutex, std::unordered_map<std::string, ShaderPipelines>>,
            static_cast<size_t>(PipelineType::SIZE)>
            vGraphicsPipelines;

        /** Stores all compute pipelines. */
        ComputePipelines computePipelines;

        /** Do not delete (free) this pointer. Renderer that owns this pipeline manager. */
        Renderer* pRenderer = nullptr;
    };
} // namespace ne
