#include "PipelineSharedPtr.h"

// Custom.
#include "render/general/pipeline/PipelineManager.h"
#include "render/general/pipeline/Pipeline.h"

namespace ne {

    PipelineSharedPtr::PipelineSharedPtr(
        std::shared_ptr<Pipeline> pPipeline, Material* pMaterialThatUsesPipeline) {
        initialize(std::move(pPipeline), pMaterialThatUsesPipeline);
    }

    PipelineSharedPtr::PipelineSharedPtr(PipelineSharedPtr&& other) noexcept { *this = std::move(other); }

    PipelineSharedPtr& PipelineSharedPtr::operator=(PipelineSharedPtr&& other) noexcept {
        if (this != &other) {
            if (other.pPipeline != nullptr) {
                pPipeline = std::move(other.pPipeline);
                other.pPipeline = nullptr;
            }

            pMaterialThatUsesPipeline = other.pMaterialThatUsesPipeline;
            pComputeShaderThatUsesPipeline = other.pComputeShaderThatUsesPipeline;

#if defined(DEBUG)
            static_assert(sizeof(PipelineSharedPtr) == 32, "consider adding new variables to move operator");
#endif

            other.pMaterialThatUsesPipeline = nullptr;
            other.pComputeShaderThatUsesPipeline = nullptr;
        }

        return *this;
    }

    bool PipelineSharedPtr::isInitialized() const { return pPipeline != nullptr; }

    void PipelineSharedPtr::clear() { clearPointerAndNotifyPipeline(); }

    void PipelineSharedPtr::set(std::shared_ptr<Pipeline> pPipeline, Material* pMaterialThatUsesPipeline) {
        clearPointerAndNotifyPipeline();
        initialize(std::move(pPipeline), pMaterialThatUsesPipeline);
    }

    Pipeline* PipelineSharedPtr::operator->() const { return pPipeline.get(); }

    void PipelineSharedPtr::clearPointerAndNotifyPipeline() {
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

    void
    PipelineSharedPtr::initialize(std::shared_ptr<Pipeline> pPipeline, Material* pMaterialThatUsesPipeline) {
        this->pPipeline = std::move(pPipeline);
        this->pMaterialThatUsesPipeline = pMaterialThatUsesPipeline;

        // Notify pipeline.
        this->pPipeline->onMaterialUsingPipeline(pMaterialThatUsesPipeline);
    }

    void PipelineSharedPtr::initialize(
        std::shared_ptr<Pipeline> pPipeline, ComputeShaderInterface* pComputeShaderThatUsesPipeline) {
        this->pPipeline = std::move(pPipeline);
        this->pComputeShaderThatUsesPipeline = pComputeShaderThatUsesPipeline;

        // Notify pipeline.
        this->pPipeline->onComputeShaderUsingPipeline(pComputeShaderThatUsesPipeline);
    }

    PipelineSharedPtr::PipelineSharedPtr(
        std::shared_ptr<Pipeline> pPipeline, ComputeShaderInterface* pComputeShaderThatUsesPipeline) {
        initialize(std::move(pPipeline), pComputeShaderThatUsesPipeline);
    }

    PipelineSharedPtr::~PipelineSharedPtr() { clearPointerAndNotifyPipeline(); }
}
