#pragma once

// Standard.
#include <array>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <atomic>

// Custom.
#include "render/general/pipeline/Pipeline.h"
#include "materials/ShaderMacro.h"

namespace ne {
    class Renderer;
    class Material;
    class MeshNode;

    /**
     * Small wrapper class for `std::shared_ptr<Pipeline>` to do some extra work
     * when started/stopped referencing a pipeline.
     */
    class PipelineSharedPtr {
    public:
        /**
         * Constructs the pointer.
         *
         * @param pPipeline                 Pipeline to store.
         * @param pMaterialThatUsesPipeline Material that stores this pointer.
         */
        explicit PipelineSharedPtr(std::shared_ptr<Pipeline> pPipeline, Material* pMaterialThatUsesPipeline) {
            initialize(std::move(pPipeline), pMaterialThatUsesPipeline);
        }

        /** Leaves the internal pointers uninitialized. */
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
                other.pMaterialThatUsesPipeline = nullptr;
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
            if (this->pPipeline != nullptr) {
                Pipeline* pPipelineRaw = this->pPipeline.get();
                this->pPipeline = nullptr; // clear shared pointer before calling notify function
                pPipelineRaw->onMaterialNoLongerUsingPipeline(pMaterialThatUsesPipeline);
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
            this->pPipeline->onMaterialUsingPipeline(pMaterialThatUsesPipeline);
        }

        /** Internally stored pipeline */
        std::shared_ptr<Pipeline> pPipeline = nullptr;

        /** Material that stores this pointer. */
        Material* pMaterialThatUsesPipeline = nullptr;
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

        /**
         * Move constructor.
         *
         * @param other Other object.
         */
        DelayedPipelineResourcesCreation(DelayedPipelineResourcesCreation&& other) noexcept {
            *this = std::move(other);
        }

        /**
         * Move assignment operator.
         *
         * @param other Other object.
         *
         * @return Resulting object.
         */
        DelayedPipelineResourcesCreation& operator=(DelayedPipelineResourcesCreation&& other) noexcept {
            if (this != &other) {
                other.bIsValid = false;
            }

            return *this;
        }

    private:
        /** Does initialization logic. */
        void initialize();

        /** Does destruction logic. */
        void destroy();

        /** Do not delete (free) this pointer. Non-owning reference to pipeline manager. */
        PipelineManager* pPipelineManager = nullptr;

        /**
         * Marks the object as valid (not moved) or invalid (moved).
         * Determines whether @ref destroy logic should be called on destruction or not.
         */
        bool bIsValid = true;
    };

    /** Base class for managing render specific pipelines. */
    class PipelineManager {
        // Pipeline notifies the manager when a material stops referencing it.
        friend class Pipeline;

        // Releases/restores internal Pipeline's resources.
        friend class DelayedPipelineResourcesCreation;

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
         * Returns the total amount of currently created graphics pipelines.
         *
         * @return Amount of currently created graphics pipelines.
         */
        size_t getCreatedGraphicsPipelineCount();

        /**
         * Returns renderer that owns this pipeline manager.
         *
         * @return Renderer.
         */
        Renderer* getRenderer() const;

    private:
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
         * Creates internal resources for all created graphics pipelines using their current configuration.
         *
         * @remark Called after @ref releaseInternalGraphicsPipelinesResources to create resources that will
         * now reference changed (new) resources.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> restoreInternalGraphicsPipelinesResources();

        /**
         * Assigns vertex and pixel shaders to create a render specific graphics pipeline (for usual
         * rendering).
         *
         * @param sVertexShaderName    Name of the compiled vertex shader (see ShaderManager::compileShaders).
         * @param sPixelShaderName     Name of the compiled pixel shader (see ShaderManager::compileShaders).
         * @param bUsePixelBlending    Whether the pixels of the mesh that uses this pipeline should blend
         * with existing pixels on back buffer or not (for transparency).
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
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
         * Array that stores a map of pipelines per pipeline type.
         * Map stores pairs of "combination of vertex/pixel(fragment) shader names" and
         * "pipelines that use that shaders".
         */
        std::array<
            std::pair<std::recursive_mutex, std::unordered_map<std::string, ShaderPipelines>>,
            static_cast<size_t>(PipelineType::SIZE)>
            vGraphicsPipelines;

        /** Do not delete (free) this pointer. Renderer that owns this pipeline manager. */
        Renderer* pRenderer = nullptr;
    };
} // namespace ne
