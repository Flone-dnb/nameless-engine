#include "ShaderCpuWriteResource.h"

// Custom.
#include "render/general/pipeline/Pipeline.h"
#include "render/general/resources/GpuResourceManager.h"
#include "render/Renderer.h"
#include "shader/general/resources/cpuwrite/DynamicCpuWriteShaderResourceArrayManager.h"

namespace ne {

    std::variant<std::unique_ptr<ShaderCpuWriteResource>, Error> ShaderCpuWriteResource::create(
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

        // Find offsets of push constants to use.
        auto result = getUintShaderConstantOffsetsFromPipelines(pipelinesToUse, sShaderResourceName);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto constantOffsets = std::get<std::unordered_map<Pipeline*, size_t>>(std::move(result));

        const auto pRenderer = (*pipelinesToUse.begin())->getRenderer();

        // Create shader resource.
        auto pShaderResource = std::unique_ptr<ShaderCpuWriteResource>(new ShaderCpuWriteResource(
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

        // Reserve a space for this shader resource's data per frame resource.
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

        return pShaderResource;
    }

    std::optional<Error>
    ShaderCpuWriteResource::changeUsedPipelines(const std::unordered_set<Pipeline*>& pipelinesToUse) {
        std::scoped_lock guard(mtxUintShaderConstantOffsets.first);

        auto result = getUintShaderConstantOffsetsFromPipelines(pipelinesToUse, getResourceName());
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
    ShaderCpuWriteResource::getUintShaderConstantOffsetsFromPipelines(
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

    ShaderCpuWriteResource::ShaderCpuWriteResource(
        const std::string& sResourceName,
        size_t iResourceDataSizeInBytes,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource,
        const std::unordered_map<Pipeline*, size_t>& uintShaderConstantOffsets)
        : ShaderResourceBase(sResourceName), onStartedUpdatingResource(onStartedUpdatingResource),
          onFinishedUpdatingResource(onFinishedUpdatingResource),
          iResourceDataSizeInBytes(iResourceDataSizeInBytes) {
        mtxUintShaderConstantOffsets.second = uintShaderConstantOffsets;
    }

    std::optional<Error> ShaderCpuWriteResource::onAfterAllPipelinesRefreshedResources() {
        std::scoped_lock guard(mtxUintShaderConstantOffsets.first);

        // Collect used pipelines.
        std::unordered_set<Pipeline*> pipelines;
        for (auto& [pPipeline, iPushConstantIndex] : mtxUintShaderConstantOffsets.second) {
            pipelines.insert(pPipeline);
        }

        // Find possibly new field offsets.
        auto result = getUintShaderConstantOffsetsFromPipelines(pipelines, getResourceName());
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
