#pragma once

// Standard.
#include <memory>

namespace ne {
    class Pipeline;
    class Material;
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
        explicit PipelineSharedPtr(std::shared_ptr<Pipeline> pPipeline, Material* pMaterialThatUsesPipeline);

        /**
         * Constructs a new pointer for a compute shader interface that uses a pipeline.
         *
         * @param pPipeline                      Pipeline to store.
         * @param pComputeShaderThatUsesPipeline Compute interface that stores this pointer.
         */
        explicit PipelineSharedPtr(
            std::shared_ptr<Pipeline> pPipeline, ComputeShaderInterface* pComputeShaderThatUsesPipeline);

        /** Leaves the internal pointers initialized as `nullptr`. */
        PipelineSharedPtr() = default;

        ~PipelineSharedPtr();

        PipelineSharedPtr(const PipelineSharedPtr&) = delete;
        PipelineSharedPtr& operator=(const PipelineSharedPtr&) = delete;

        /**
         * Move constructor.
         *
         * @param other other object.
         */
        PipelineSharedPtr(PipelineSharedPtr&& other) noexcept;

        /**
         * Move assignment.
         *
         * @param other other object.
         *
         * @return Result of move assignment.
         */
        PipelineSharedPtr& operator=(PipelineSharedPtr&& other) noexcept;

        /**
         * Tells whether the internal pipeline was set or not.
         *
         * @return Whether the internal pipeline was set or not.
         */
        bool isInitialized() const;

        /** Clears the pointer (sets to `nullptr`). */
        void clear();

        /**
         * Changes stored pipeline to the specified one.
         *
         * @param pPipeline                 Pipeline to use.
         * @param pMaterialThatUsesPipeline Material that stores this pointer.
         */
        void set(std::shared_ptr<Pipeline> pPipeline, Material* pMaterialThatUsesPipeline);

        /**
         * Returns pointer to underlying pipeline.
         *
         * @warning Do not delete returned pointer.
         *
         * @return Raw pointer to the underlying pipeline.
         */
        inline Pipeline* getPipeline() const { return pPipeline.get(); }

        /**
         * Access operator.
         *
         * @return Raw pointer to the underlying pipeline.
         */
        Pipeline* operator->() const;

    private:
        /** Clears stored shared pointer and notifies the Pipeline that we no longer reference it. */
        void clearPointerAndNotifyPipeline();

        /**
         * Initializes internal state.
         *
         * @param pPipeline                 Pipeline to store.
         * @param pMaterialThatUsesPipeline Material that stores this pointer.
         */
        void initialize(std::shared_ptr<Pipeline> pPipeline, Material* pMaterialThatUsesPipeline);

        /**
         * Initializes internal state.
         *
         * @param pPipeline                      Pipeline to store.
         * @param pComputeShaderThatUsesPipeline Compute interface that stores this pointer.
         */
        void initialize(
            std::shared_ptr<Pipeline> pPipeline, ComputeShaderInterface* pComputeShaderThatUsesPipeline);

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
}
