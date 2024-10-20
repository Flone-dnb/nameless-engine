#include "ShaderCpuWriteResourceBinding.h"

// Custom.
#include "render/general/pipeline/Pipeline.h"
#include "render/general/resource/GpuResourceManager.h"
#include "render/Renderer.h"
#include "shader/general/resource/cpuwrite/DynamicCpuWriteShaderResourceArrayManager.h"
#include "render/general/pipeline/PipelineManager.h"

namespace ne {

    std::variant<std::unique_ptr<ShaderCpuWriteResourceBinding>, Error> ShaderCpuWriteResourceBinding::create(
        const std::string& sShaderResourceName,
        const std::string& sResourceAdditionalInfo,
        size_t iResourceSizeInBytes,
        const std::unordered_set<Pipeline*>& pipelinesToUse,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }
        const auto pRenderer = (*pipelinesToUse.begin())->getRenderer();

        // Make sure no pipeline will re-create its internal resources because we will now reference
        // pipeline's internal resources.
        // After we create a new shader resource binding object we can release the mutex
        // since shader resource bindings are notified after pipelines re-create their internal resources.
        const auto pMtxGraphicsPipelines = pRenderer->getPipelineManager()->getGraphicsPipelines();
        std::scoped_lock pipelinesGuard(pMtxGraphicsPipelines->first);

        // Find offsets of push constants to use.
        auto result = getUintShaderConstantOffsetsFromPipelines(pipelinesToUse, sShaderResourceName);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto constantOffsets = std::get<std::unordered_map<Pipeline*, size_t>>(std::move(result));

        // Create shader resource.
        auto pShaderResource =
            std::unique_ptr<ShaderCpuWriteResourceBinding>(new ShaderCpuWriteResourceBinding(
                sShaderResourceName,
                iResourceSizeInBytes,
                onStartedUpdatingResource,
                onFinishedUpdatingResource,
                constantOffsets));

        // Get resource manager.
        const auto pResourceManager = pRenderer->getResourceManager();
        if (pResourceManager == nullptr) [[unlikely]] {
            return Error("resource manager is `nullptr`");
        }

        // Get shader resource array manager.
        const auto pShaderResourceArrayManager =
            pResourceManager->getDynamicCpuWriteShaderResourceArrayManager();
        if (pShaderResourceArrayManager == nullptr) [[unlikely]] {
            return Error("shader resource array manager is `nullptr`");
        }

        // Reserve a space for this shader resource's data per frame resource
        // (one slot per frame in-flight because we operate on CPU-write resources
        // so we have to keep N copies of the data in case it's changing to avoid stopping the GPU).
        for (unsigned int i = 0; i < FrameResourceManager::getFrameResourceCount(); i++) {
            auto result = pShaderResourceArrayManager->reserveSlotsInArray(pShaderResource.get());
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            pShaderResource->vResourceData[i] =
                std::get<std::unique_ptr<DynamicCpuWriteShaderResourceArraySlot>>(std::move(result));
        }

        // At this point we can release the pipelines mutex.

        return pShaderResource;
    }

    std::optional<Error>
    ShaderCpuWriteResourceBinding::changeUsedPipelines(const std::unordered_set<Pipeline*>& pipelinesToUse) {
        std::scoped_lock guard(mtxUintShaderConstantOffsets.first);

        auto result = getUintShaderConstantOffsetsFromPipelines(pipelinesToUse, getShaderResourceName());
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        mtxUintShaderConstantOffsets.second =
            std::get<std::unordered_map<Pipeline*, size_t>>(std::move(result));

        return {};
    }

    std::variant<std::unordered_map<Pipeline*, size_t>, Error>
    ShaderCpuWriteResourceBinding::getUintShaderConstantOffsetsFromPipelines(
        const std::unordered_set<Pipeline*>& pipelines, const std::string& sFieldName) {
        // Make sure at least one pipeline is specified.
        if (pipelines.empty()) [[unlikely]] {
            return Error(std::format(
                "expected at least one pipeline to be specified to find a field named \"{}\"", sFieldName));
        }

        std::unordered_map<Pipeline*, size_t> offsets;

        // Find constants.
        for (const auto& pPipeline : pipelines) {
            auto result = pPipeline->getUintConstantOffset(sFieldName);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            offsets[pPipeline] = std::get<size_t>(result);
        }

        return offsets;
    }

    ShaderCpuWriteResourceBinding::ShaderCpuWriteResourceBinding(
        const std::string& sShaderResourceName,
        size_t iResourceDataSizeInBytes,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource,
        const std::unordered_map<Pipeline*, size_t>& uintShaderConstantOffsets)
        : ShaderResourceBindingBase(sShaderResourceName),
          onStartedUpdatingResource(onStartedUpdatingResource),
          onFinishedUpdatingResource(onFinishedUpdatingResource),
          iResourceDataSizeInBytes(iResourceDataSizeInBytes) {
        mtxUintShaderConstantOffsets.second = uintShaderConstantOffsets;
    }

    std::optional<Error> ShaderCpuWriteResourceBinding::onAfterAllPipelinesRefreshedResources() {
        std::scoped_lock guard(mtxUintShaderConstantOffsets.first);

        // Collect used pipelines.
        std::unordered_set<Pipeline*> pipelines;
        for (auto& [pPipeline, iPushConstantIndex] : mtxUintShaderConstantOffsets.second) {
            pipelines.insert(pPipeline);
        }

        // Find possibly new field offsets.
        auto result = getUintShaderConstantOffsetsFromPipelines(pipelines, getShaderResourceName());
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }

        // Save offsets.
        mtxUintShaderConstantOffsets.second =
            std::get<std::unordered_map<Pipeline*, size_t>>(std::move(result));

        return {};
    }
}
