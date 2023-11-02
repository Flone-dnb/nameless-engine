#include "shader/ComputeShaderInterface.h"

// Custom.
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "shader/hlsl/HlslComputeShaderInterface.h"
#endif
#include "render/vulkan/VulkanRenderer.h"

namespace ne {

    ComputeShaderInterface::ComputeShaderInterface(
        Renderer* pRenderer, const std::string& sComputeShaderName, bool bRunBeforeFrameRendering)
        : bRunBeforeFrameRendering(bRunBeforeFrameRendering), sComputeShaderName(sComputeShaderName) {
        this->pRenderer = pRenderer;
    }

    ComputeShaderInterface::~ComputeShaderInterface() {
        // Explicitly reset used pipeline to notify pipeline manager
        // in case the manager would want to use our member functions so that
        // these calls will still be valid.
        pPipeline.clear();
    }

    std::variant<std::unique_ptr<ComputeShaderInterface>, Error>
    ComputeShaderInterface::createUsingGraphicsQueue(
        Renderer* pRenderer, const std::string& sCompiledComputeShaderName, bool bRunBeforeFrameRendering) {
        return createRenderSpecificInterface(pRenderer, sCompiledComputeShaderName, bRunBeforeFrameRendering);
    }

    std::unique_ptr<ComputeShaderInterface>
    ComputeShaderInterface::createPartiallyInitializedRenderSpecificInterface(
        Renderer* pRenderer, const std::string& sComputeShaderName, bool bRunBeforeFrameRendering) {
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            return std::unique_ptr<HlslComputeShaderInterface>(
                new HlslComputeShaderInterface(pRenderer, sComputeShaderName, bRunBeforeFrameRendering));
        }
#endif

        Error error("unsupported renderer");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }

    std::variant<std::unique_ptr<ComputeShaderInterface>, Error>
    ComputeShaderInterface::createRenderSpecificInterface(
        Renderer* pRenderer, const std::string& sComputeShaderName, bool bRunBeforeFrameRendering) {
        // Create a new partially initialized render-specific interface.
        auto pNewInterface = createPartiallyInitializedRenderSpecificInterface(
            pRenderer, sComputeShaderName, bRunBeforeFrameRendering);

        // Get pipeline manager.
        const auto pPipelineManager = pRenderer->getPipelineManager();

        // Get a compute pipeline for the specified shader.
        auto result = pPipelineManager->computePipelines.getComputePipelineForShader(
            pPipelineManager, pNewInterface.get());
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        pNewInterface->pPipeline = std::get<PipelineSharedPtr>(std::move(result));

        return pNewInterface;
    }

    void ComputeShaderInterface::submitForExecution(
        unsigned int iThreadGroupCountX, unsigned int iThreadGroupCountY, unsigned int iThreadGroupCountZ) {
        // Save thread group count.
        this->iThreadGroupCountX = iThreadGroupCountX;
        this->iThreadGroupCountY = iThreadGroupCountY;
        this->iThreadGroupCountZ = iThreadGroupCountZ;

        // Get pipeline manager.
        const auto pPipelineManager = pRenderer->getPipelineManager();

        // Queue shader.
        std::optional<Error> optionalError;
        if (bRunBeforeFrameRendering) {
            optionalError =
                pPipelineManager->computePipelines.queueShaderExecutionOnGraphicsQueuePreFrame(this);
        } else {
            optionalError =
                pPipelineManager->computePipelines.queueShaderExecutionOnGraphicsQueuePostFrame(this);
        }
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }

        // The renderer will call a non-virtual function on derived (render-specific) class for graphics
        // queue dispatch during the draw operation.
    }

    std::string ComputeShaderInterface::getComputeShaderName() const { return sComputeShaderName; }

    Pipeline* ComputeShaderInterface::getUsedPipeline() const { return pPipeline.getPipeline(); }

    Renderer* ComputeShaderInterface::getRenderer() { return pRenderer; }

    Pipeline* ComputeShaderInterface::getPipeline() const { return pPipeline.getPipeline(); }
}
