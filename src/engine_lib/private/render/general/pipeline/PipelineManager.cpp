#include "PipelineManager.h"

// Custom.
#include "io/Logger.h"
#include "render/Renderer.h"
#include "render/general/resource/GpuResourceManager.h"
#include "shader/ComputeShaderInterface.h"
#include "shader/general/resource/LightingShaderResourceManager.h"
#include "shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBindingManager.h"
#include "shader/general/resource/binding/texture/ShaderTextureResourceBindingManager.h"
#include "shader/general/resource/binding/global/GlobalShaderResourceBindingManager.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"

namespace ne {
    PipelineManager::PipelineManager(Renderer* pRenderer) : pRenderer(pRenderer) {}

    std::optional<Error> PipelineManager::bindBuffersToAllVulkanPipelinesIfUsed(
        const std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()>& vResources,
        const std::string& sShaderResourceName,
        VkDescriptorType descriptorType) {
        std::scoped_lock pipelinesGuard(mtxGraphicsPipelines.first);

        // Iterate over graphics pipelines of all types.
        for (auto& pipelinesOfSpecificType : mtxGraphicsPipelines.second.vPipelineTypes) {

            // Iterate over all active shader combinations.
            for (const auto& [sShaderNames, pipelines] : pipelinesOfSpecificType) {

                // Iterate over all active unique material macros combinations.
                for (const auto& [materialMacros, pPipeline] : pipelines.shaderPipelines) {
                    // Get pipeline.
                    const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline.get());
                    if (pVulkanPipeline == nullptr) [[unlikely]] {
                        return Error("expected a Vulkan pipeline");
                    }

                    // Rebind resources to pipeline.
                    auto optionalError =
                        pVulkanPipeline->bindBuffersIfUsed(vResources, sShaderResourceName, descriptorType);
                    if (optionalError.has_value()) [[unlikely]] {
                        optionalError->addCurrentLocationToErrorStack();
                        return optionalError;
                    }
                }
            }
        }

        return {};
    }

    std::optional<Error> PipelineManager::bindImageToAllVulkanPipelinesIfUsed(
        GpuResource* pImageResourceToBind,
        const std::string& sShaderResourceName,
        VkDescriptorType descriptorType,
        VkImageLayout imageLayout,
        VkSampler pSampler) {
        std::scoped_lock pipelinesGuard(mtxGraphicsPipelines.first);

        // Iterate over graphics pipelines of all types.
        for (auto& pipelinesOfSpecificType : mtxGraphicsPipelines.second.vPipelineTypes) {

            // Iterate over all active shader combinations.
            for (const auto& [sShaderNames, pipelines] : pipelinesOfSpecificType) {

                // Iterate over all active unique material macros combinations.
                for (const auto& [materialMacros, pPipeline] : pipelines.shaderPipelines) {
                    // Get pipeline.
                    const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline.get());
                    if (pVulkanPipeline == nullptr) [[unlikely]] {
                        return Error("expected a Vulkan pipeline");
                    }

                    // Rebind image to pipeline.
                    auto optionalError = pVulkanPipeline->bindImageIfUsed(
                        pImageResourceToBind, sShaderResourceName, descriptorType, imageLayout, pSampler);
                    if (optionalError.has_value()) [[unlikely]] {
                        optionalError->addCurrentLocationToErrorStack();
                        return optionalError;
                    }
                }
            }
        }

        return {};
    }

    DelayedPipelineResourcesCreation
    PipelineManager::clearGraphicsPipelinesInternalResourcesAndDelayRestoring() {
        return DelayedPipelineResourcesCreation(this);
    }

    std::variant<PipelineSharedPtr, Error> PipelineManager::findOrCreatePipeline(
        std::unordered_map<std::string, ShaderPipelines>& pipelines,
        const std::string& sKeyToLookFor,
        const std::set<ShaderMacro>& macrosToLookFor,
        std::unique_ptr<PipelineConfiguration> pPipelineConfiguration,
        Material* pMaterial) {
        // Find pipeline for the specified shader(s).
        const auto foundPipelineIt = pipelines.find(sKeyToLookFor);
        if (foundPipelineIt == pipelines.end()) {
            // There are no pipelines that use this shader combination.
            return createGraphicsPipelineForMaterial(
                pipelines, sKeyToLookFor, macrosToLookFor, std::move(pPipelineConfiguration), pMaterial);
        }

        // Check if we already have a pipeline that uses the same shader macro combination.
        const auto shaderPipelinesIt = foundPipelineIt->second.shaderPipelines.find(macrosToLookFor);
        if (shaderPipelinesIt == foundPipelineIt->second.shaderPipelines.end()) {
            // There is no pipeline that uses the same shader macros.
            return createGraphicsPipelineForMaterial(
                pipelines, sKeyToLookFor, macrosToLookFor, std::move(pPipelineConfiguration), pMaterial);
        }

        // Just create a new shared pointer to already existing pipeline.
        return PipelineSharedPtr(shaderPipelinesIt->second, pMaterial);
    }

    std::variant<PipelineSharedPtr, Error> PipelineManager::getGraphicsPipelineForMaterial(
        std::unique_ptr<PipelineConfiguration> pPipelineConfiguration, Material* pMaterial) {
        // Self check: make sure settings are not `nullptr`.
        if (pPipelineConfiguration == nullptr) [[unlikely]] {
            return Error("settings cannot be `nullptr`");
        }

        const auto additionalVertexShaderMacros = pPipelineConfiguration->getAdditionalVertexShaderMacros();
        const auto additionalPixelShaderMacros = pPipelineConfiguration->getAdditionalPixelShaderMacros();
        {
            // Self check: make sure vertex macros have "VS_" prefix and pixel macros "PS_" prefix.
            const auto vVertexMacros = convertShaderMacrosToText(additionalVertexShaderMacros);
            const auto vPixelMacros = convertShaderMacrosToText(additionalPixelShaderMacros);
            for (const auto& sVertexMacro : vVertexMacros) {
                if (!sVertexMacro.starts_with("VS_")) [[unlikely]] {
                    return Error(std::format(
                        "vertex shader macro \"{}\" that should start with \"VS_\" prefix", sVertexMacro));
                }
            }
            for (const auto& sPixelMacro : vPixelMacros) {
                if (!sPixelMacro.starts_with("PS_")) [[unlikely]] {
                    return Error(std::format(
                        "pixel/fragment shader macro \"{}\" that should start with \"PS_\" prefix",
                        sPixelMacro));
                }
            }
        }

        // Combine vertex/pixel macros of material into one set.
        std::set<ShaderMacro> additionalVertexAndPixelShaderMacros = additionalVertexShaderMacros;
        for (const auto& macro : additionalPixelShaderMacros) {
            additionalVertexAndPixelShaderMacros.insert(macro);
        }

        // Prepare to find or create a pipeline.
        std::scoped_lock guard(mtxGraphicsPipelines.first);

        const auto sKeyToLookFor = Pipeline::combineShaderNames(
            pPipelineConfiguration->getVertexShaderName(), pPipelineConfiguration->getPixelShaderName());

        auto optionalGraphicsType = pPipelineConfiguration->getGraphicsType();
        if (!optionalGraphicsType.has_value()) [[unlikely]] {
            return Error(std::format("expected pipeline \"{}\" to be a graphics pipeline", sKeyToLookFor));
        }
        const auto iPipelineTypeIndex = static_cast<size_t>(optionalGraphicsType.value());

        // Find existing or create a new pipeline.
        return findOrCreatePipeline(
            mtxGraphicsPipelines.second.vPipelineTypes[iPipelineTypeIndex],
            sKeyToLookFor,
            additionalVertexAndPixelShaderMacros,
            std::move(pPipelineConfiguration),
            pMaterial);
    }

    PipelineManager::~PipelineManager() {
        // Make sure all graphics pipelines were destroyed.
        const auto iCreatedGraphicsPipelineCount = getCurrentGraphicsPipelineCount();
        if (iCreatedGraphicsPipelineCount != 0) [[unlikely]] {
            // Log error.
            Logger::get().error(std::format(
                "pipeline manager is being destroyed but {} graphics pipeline(s) exist:",
                iCreatedGraphicsPipelineCount));

            std::scoped_lock guard(mtxGraphicsPipelines.first);

            // Iterate over all graphics pipelines (of all types).
            for (auto& pipelinesOfSpecificType : mtxGraphicsPipelines.second.vPipelineTypes) {

                // Iterate over all active shader combinations.
                for (const auto& [sShaderNames, pipelines] : pipelinesOfSpecificType) {
                    Logger::get().error(std::format(
                        "- \"{}\" ({} pipeline(s))", sShaderNames, pipelines.shaderPipelines.size()));

                    // Iterate over all pipelines that use these shaders.
                    for (const auto& [macros, pPipeline] : pipelines.shaderPipelines) {
                        // Convert macros to text.
                        const auto vMacros = convertShaderMacrosToText(macros);
                        std::string sMacros;
                        if (!vMacros.empty()) {
                            for (const auto& sMacro : vMacros) {
                                sMacros += std::format("{}, ", sMacro);
                            }
                            sMacros.pop_back(); // pop space
                            sMacros.pop_back(); // pop comma
                        }

                        // Log macros.
                        Logger::get().error(std::format(
                            "-- macros: {}, active references: {} (including this manager)",
                            sMacros,
                            pPipeline.use_count()));
                    }
                }
            }
        }

        // Make sure all compute pipelines were destroyed.
        const auto iActiveComputePipelines = getCurrentComputePipelineCount();
        if (iActiveComputePipelines != 0) [[unlikely]] {
            // Log error.
            Logger::get().error(std::format(
                "pipeline manager is being destroyed but {} compute pipeline(s) still exist",
                iActiveComputePipelines));
        }
    }

    std::variant<PipelineSharedPtr, Error> PipelineManager::createGraphicsPipelineForMaterial(
        std::unordered_map<std::string, ShaderPipelines>& pipelines,
        const std::string& sShaderNames,
        const std::set<ShaderMacro>& macrosToUse,
        std::unique_ptr<PipelineConfiguration> pPipelineConfiguration,
        Material* pMaterial) {
        // Create pipeline.
        auto result = Pipeline::createGraphicsPipeline(pRenderer, this, std::move(pPipelineConfiguration));
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pPipeline = std::get<std::shared_ptr<Pipeline>>(std::move(result));

        // See if we already have pipelines that uses these shaders.
        const auto pipelinesIt = pipelines.find(sShaderNames);
        if (pipelinesIt == pipelines.end()) {
            // This is the only pipeline that uses these shaders.

            // Prepare pipeline map.
            ShaderPipelines pipelineMacros;
            pipelineMacros.shaderPipelines[macrosToUse] = pPipeline;

            // Add to all pipelines and return a shared pointer.
            pipelines[sShaderNames] = std::move(pipelineMacros);
        } else {
            // Make sure there are no pipelines that uses the same macros (and shaders).
            const auto shaderPipelinesIt = pipelinesIt->second.shaderPipelines.find(macrosToUse);
            if (shaderPipelinesIt != pipelinesIt->second.shaderPipelines.end()) [[unlikely]] {
                return Error(std::format(
                    "expected that there are no pipelines that use the same material macros {} for shaders "
                    "{}",
                    ShaderMacroConfigurations::convertConfigurationToText(macrosToUse),
                    sShaderNames));
            }

            // Add pipeline.
            pipelinesIt->second.shaderPipelines[macrosToUse] = pPipeline;
        }

        // Notify global shader resource binding manager so that global resources can be binded.
        auto optionalError =
            getRenderer()->getGlobalShaderResourceBindingManager()->onNewGraphicsPipelineCreated(
                pPipeline.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Bind GPU lighting resources to pipeline descriptors (if this pipeline uses them).
        optionalError =
            getRenderer()->getLightingShaderResourceManager()->updateDescriptorsForPipelineResource(
                pPipeline.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        // Bind shadow maps (if this pipeline uses them).
        optionalError = getRenderer()->getResourceManager()->getShadowMapManager()->bindShadowMapsToPipeline(
            pPipeline.get());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError.value();
        }

        return PipelineSharedPtr(pPipeline, pMaterial);
    }

    size_t PipelineManager::getCurrentGraphicsPipelineCount() {
        size_t iTotalCount = 0;

        std::scoped_lock guard(mtxGraphicsPipelines.first);

        // Count  pipelines.
        for (const auto& pipelinesOfSpecificType : mtxGraphicsPipelines.second.vPipelineTypes) {
            for (const auto& [sShaderNames, pipelines] : pipelinesOfSpecificType) {
                iTotalCount += pipelines.shaderPipelines.size();
            }
        }

        return iTotalCount;
    }

    size_t PipelineManager::getCurrentComputePipelineCount() {
        return computePipelines.getComputePipelineCount();
    }

    Renderer* PipelineManager::getRenderer() const { return pRenderer; }

    std::optional<Error> PipelineManager::releaseInternalGraphicsPipelinesResources() {
        mtxGraphicsPipelines.first.lock(); // lock until resources where not restored

        for (auto& pipelinesOfSpecificType : mtxGraphicsPipelines.second.vPipelineTypes) {

            // Iterate over all active shader combinations.
            for (const auto& [sShaderNames, pipelines] : pipelinesOfSpecificType) {

                // Iterate over all active material macros combinations.
                for (const auto& [materialMacros, pPipeline] : pipelines.shaderPipelines) {
                    // Release resources.
                    auto optionalError = pPipeline->releaseInternalResources();
                    if (optionalError.has_value()) [[unlikely]] {
                        auto error = std::move(optionalError.value());
                        error.addCurrentLocationToErrorStack();
                        return error;
                    }
                }
            }
        }

        return {};
    }

    std::optional<Error> PipelineManager::restoreInternalGraphicsPipelinesResources() {
        for (auto& pipelinesOfSpecificType : mtxGraphicsPipelines.second.vPipelineTypes) {

            // Iterate over all active shader combinations.
            for (const auto& [sShaderNames, pipelines] : pipelinesOfSpecificType) {

                // Iterate over all active material macros combinations.
                for (const auto& [materialMacros, pPipeline] : pipelines.shaderPipelines) {
                    // Restore resources.
                    auto optionalError = pPipeline->restoreInternalResources();
                    if (optionalError.has_value()) [[unlikely]] {
                        auto error = std::move(optionalError.value());
                        error.addCurrentLocationToErrorStack();
                        return error;
                    }
                }
            }
        }

        // Log notification start.
        Logger::get().info("notifying renderer's subsystems about refreshed pipeline resources...");
        Logger::get().flushToDisk(); // flush to disk to see if we crashed while notifying

        // Rebind global shader resources.
        {
            auto optionalError = getRenderer()
                                     ->getGlobalShaderResourceBindingManager()
                                     ->onAllGraphicsPipelinesRecreatedInternalResources();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        // Re-bind shader CPU write resources.
        {
            const auto pMtxResources = getRenderer()->getShaderCpuWriteResourceManager()->getResources();
            std::scoped_lock shaderResourceGuard(pMtxResources->first);

            for (const auto& [pRawResource, pResource] : pMtxResources->second.all) {
                // Notify (cast to parent for `friend class` to work).
                auto optionalError = static_cast<ShaderResourceBindingBase*>(pResource.get())
                                         ->onAfterAllPipelinesRefreshedResources();
                if (optionalError.has_value()) [[unlikely]] {
                    optionalError->addCurrentLocationToErrorStack();
                    return optionalError;
                }
            }
        }

        // Re-bind shader texture resources.
        {
            const auto pMtxResources = getRenderer()->getShaderTextureResourceManager()->getResources();
            std::scoped_lock shaderResourceGuard(pMtxResources->first);

            for (const auto& [pRawResource, pResource] : pMtxResources->second) {
                // Notify.
                auto optionalError = pResource->onAfterAllPipelinesRefreshedResources();
                if (optionalError.has_value()) [[unlikely]] {
                    optionalError->addCurrentLocationToErrorStack();
                    return optionalError;
                }
            }
        }

        // Re-bind lighting shader resources.
        auto optionalError =
            getRenderer()->getLightingShaderResourceManager()->bindDescriptorsToRecreatedPipelineResources();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Re-bind all shadow maps.
        optionalError =
            pRenderer->getResourceManager()->getShadowMapManager()->bindShadowMapsToAllPipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        // Log notification end.
        Logger::get().info("finished notifying all renderer's subsystems about refreshed pipeline resources");
        Logger::get().flushToDisk();

        // Unlock the mutex because all pipeline resources were re-created.
        mtxGraphicsPipelines.first.unlock();

        return {};
    }

    void PipelineManager::onPipelineNoLongerUsedByMaterial(const std::string& sPipelineIdentifier) {
        // Iterate over all types of pipelines (opaque, transparent).
        bool bFound = false;
        for (auto& pipelinesOfSpecificType : mtxGraphicsPipelines.second.vPipelineTypes) {
            // Find this pipeline.
            const auto it = pipelinesOfSpecificType.find(sPipelineIdentifier);
            if (it == pipelinesOfSpecificType.end()) {
                continue;
            }

            // Mark that we found something.
            bFound = true;

            // Remove pipelines that are no longer used.
            std::erase_if(
                it->second.shaderPipelines, [](const auto& pair) { return pair.second.use_count() == 1; });

            // Remove entry for pipelines of this shader combination if there are no pipelines.
            if (it->second.shaderPipelines.empty()) {
                pipelinesOfSpecificType.erase(it);
            }
        }

        // Self check: make sure we found something.
        if (!bFound) [[unlikely]] {
            Logger::get().error(
                std::format("unable to find the specified pipeline \"{}\"", sPipelineIdentifier));
        }
    }

    void PipelineManager::onPipelineNoLongerUsedByComputeShaderInterface(
        const std::string& sComputeShaderName, ComputeShaderInterface* pComputeShaderInterface) {
        auto optionalError = computePipelines.onPipelineNoLongerUsedByComputeShaderInterface(
            sComputeShaderName, pComputeShaderInterface);
        if (optionalError.has_value()) [[unlikely]] {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    void DelayedPipelineResourcesCreation::initialize() {
        const auto pRenderer = pPipelineManager->getRenderer();

        // Make sure no drawing is happening and the GPU is not referencing any resources.
        std::scoped_lock guard(
            *pRenderer->getRenderResourcesMutex()); // we don't need to hold this lock until destroyed since
                                                    // pipeline manager will hold its lock until all resources
                                                    // are not restored (which will not allow new frames to be
                                                    // rendered)
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Release resources.
        auto optionalError = pPipelineManager->releaseInternalGraphicsPipelinesResources();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    void DelayedPipelineResourcesCreation::destroy() {
        // Restore resources.
        auto optionalError = pPipelineManager->restoreInternalGraphicsPipelinesResources();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    size_t PipelineManager::ComputePipelines::getComputePipelineCount() {
        std::scoped_lock guard(mtxResources.first);
        return mtxResources.second.pipelines.size();
    }

    std::variant<PipelineSharedPtr, Error> PipelineManager::ComputePipelines::getComputePipelineForShader(
        PipelineManager* pPipelineManager, ComputeShaderInterface* pComputeShaderInterface) {
        std::scoped_lock guard(mtxResources.first);

        // See if a pipeline for this shader already exist.
        const auto pipelineIt =
            mtxResources.second.pipelines.find(pComputeShaderInterface->getComputeShaderName());
        if (pipelineIt == mtxResources.second.pipelines.end()) {
            // Create a new compute pipeline.
            auto result = Pipeline::createComputePipeline(
                pPipelineManager->getRenderer(),
                pPipelineManager,
                pComputeShaderInterface->getComputeShaderName());
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            auto pPipeline = std::get<std::shared_ptr<Pipeline>>(std::move(result));

            // Add this new pipeline to the map of pipelines.
            mtxResources.second.pipelines[pComputeShaderInterface->getComputeShaderName()] = pPipeline;

            // Return newly created pipeline.
            return PipelineSharedPtr(pPipeline, pComputeShaderInterface);
        }

        // Return found pipeline.
        return PipelineSharedPtr(pipelineIt->second, pComputeShaderInterface);
    }

    std::optional<Error> PipelineManager::ComputePipelines::onPipelineNoLongerUsedByComputeShaderInterface(
        const std::string& sComputeShaderName, ComputeShaderInterface* pComputeShaderInterface) {
        std::scoped_lock guard(mtxResources.first);

        // Find a pipeline for the specified shader.
        const auto computeIt = mtxResources.second.pipelines.find(sComputeShaderName);
        if (computeIt == mtxResources.second.pipelines.end()) [[unlikely]] {
            return Error(
                std::format("failed to find a compute pipeline for shader \"{}\"", sComputeShaderName));
        }

        // Make sure this pipeline is no longer used.
        if (computeIt->second.use_count() != 1) {
            // Still used by someone else (not including us).
            return {};
        }

        // Save raw pointer to pipeline.
        const auto pPipelineRaw = computeIt->second.get();

        // Remove pipeline from "queued" pipelines arrays.
        for (auto& stage : mtxResources.second.queuedComputeShaders.vGraphicsQueueStagesGroups) {
            for (auto& group : stage) {
                group.erase(pPipelineRaw);
            }
        }
#if defined(DEBUG) && defined(WIN32)
        static_assert(sizeof(QueuedForExecutionComputeShaders) == 160, "erase from new arrays");
#elif defined(DEBUG)
        static_assert(sizeof(QueuedForExecutionComputeShaders) == 112, "erase from new arrays");
#endif

        // Destroy pipeline.
        mtxResources.second.pipelines.erase(computeIt);

        return {};
    }

    std::optional<Error> PipelineManager::ComputePipelines::queueShaderExecutionOnGraphicsQueue(
        ComputeShaderInterface* pComputeShaderInterface) {
        std::scoped_lock guard(mtxResources.first);

        // Prepare a reference to make code simpler.
        auto& queued = mtxResources.second.queuedComputeShaders;

        // Prepare stage to modify.
        auto& stageToUse = queued.vGraphicsQueueStagesGroups[static_cast<size_t>(
            pComputeShaderInterface->getExecutionStage())];

        // Prepare group to modify.
        auto& groupToUse = stageToUse[static_cast<size_t>(pComputeShaderInterface->getExecutionGroup())];

        // Add to be executed.
        auto optionalError = queueComputeShaderInterfaceForExecution(groupToUse, pComputeShaderInterface);
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> PipelineManager::ComputePipelines::queueComputeShaderInterfaceForExecution(
        std::unordered_map<Pipeline*, std::unordered_set<ComputeShaderInterface*>>& pipelineShaders,
        ComputeShaderInterface* pComputeShaderInterface) {
        // Expecting that resources mutex is locked.

        // Get pipeline of this compute shader interface.
        const auto pPipeline = pComputeShaderInterface->getUsedPipeline();
        if (pPipeline == nullptr) [[unlikely]] {
            return Error(std::format(
                "expected the pipeline of the compute shader interface \"{}\" to be valid",
                pComputeShaderInterface->getComputeShaderName()));
        }

        // See if there are already some interfaces that were queued using this pipeline.
        const auto pipelineShadersIt = pipelineShaders.find(pPipeline);
        if (pipelineShadersIt == pipelineShaders.end()) {
            // Add a new entry.
            pipelineShaders[pPipeline] = {pComputeShaderInterface};

            // Done.
            return {};
        }

        // Self check: make sure array of interfaces is not empty since we have a pipeline entry.
        if (pipelineShadersIt->second.empty()) [[unlikely]] {
            return Error(std::format(
                "array of compute interfaces was empty but a pipeline entry was still valid while compute "
                "shader interface \"{}\" was being queued for execution",
                pComputeShaderInterface->getComputeShaderName()));
        }

        // Add a new interface.
        pipelineShadersIt->second.insert(pComputeShaderInterface);

        return {};
    }

} // namespace ne
