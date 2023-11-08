#include "Pipeline.h"

// Custom.
#include "render/Renderer.h"
#include "io/Logger.h"
#include "render/general/pipeline/PipelineManager.h"
#include "material/Material.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "render/directx/pipeline/DirectXPso.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"

namespace ne {

    Pipeline::Pipeline(
        Renderer* pRenderer,
        PipelineManager* pPipelineManager,
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        const std::string& sComputeShaderName,
        bool bUsePixelBlending)
        : ShaderUser(pRenderer->getShaderManager()), sVertexShaderName(sVertexShaderName),
          sPixelShaderName(sPixelShaderName), sComputeShaderName(sComputeShaderName) {
        this->pRenderer = pRenderer;
        this->pPipelineManager = pPipelineManager;

        bIsUsingPixelBlending = bUsePixelBlending;
    }

    void
    Pipeline::saveUsedShaderConfiguration(ShaderType shaderType, std::set<ShaderMacro>&& fullConfiguration) {
        usedShaderConfiguration[shaderType] = std::move(fullConfiguration);
    }

    std::string
    Pipeline::combineShaderNames(const std::string& sVertexShaderName, const std::string& sPixelShaderName) {
        if (sPixelShaderName.empty()) {
            return sVertexShaderName;
        }
        return sVertexShaderName + " / " + sPixelShaderName;
    }

    std::string Pipeline::getVertexShaderName() { return sVertexShaderName; }

    std::string Pipeline::getPixelShaderName() { return sPixelShaderName; }

    std::optional<std::set<ShaderMacro>> Pipeline::getCurrentShaderConfiguration(ShaderType shaderType) {
        auto it = usedShaderConfiguration.find(shaderType);
        if (it == usedShaderConfiguration.end()) {
            return {};
        }

        return it->second;
    }

    bool Pipeline::isUsingPixelBlending() const { return bIsUsingPixelBlending; }

    std::pair<std::mutex, std::unordered_set<Material*>>* Pipeline::getMaterialsThatUseThisPipeline() {
        return &mtxMaterialsThatUseThisPipeline;
    }

    std::variant<std::shared_ptr<Pipeline>, Error> Pipeline::createGraphicsPipeline(
        Renderer* pRenderer,
        PipelineManager* pPipelineManager,
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::set<ShaderMacro>& additionalPixelShaderMacros) {
        std::shared_ptr<Pipeline> pCreatedPipeline = nullptr;
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            // Create DirectX PSO.
            auto result = DirectXPso::createGraphicsPso(
                pRenderer,
                pPipelineManager,
                sVertexShaderName,
                sPixelShaderName,
                bUsePixelBlending,
                additionalVertexShaderMacros,
                additionalPixelShaderMacros);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            pCreatedPipeline =
                std::dynamic_pointer_cast<Pipeline>(std::get<std::shared_ptr<DirectXPso>>(result));
        }
#endif

        if (pCreatedPipeline == nullptr && dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            // Create Vulkan pipeline.
            auto result = VulkanPipeline::createGraphicsPipeline(
                pRenderer,
                pPipelineManager,
                sVertexShaderName,
                sPixelShaderName,
                bUsePixelBlending,
                additionalVertexShaderMacros,
                additionalPixelShaderMacros);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            pCreatedPipeline =
                std::dynamic_pointer_cast<Pipeline>(std::get<std::shared_ptr<VulkanPipeline>>(result));
        }

        // Make sure the pipeline was created.
        if (pCreatedPipeline == nullptr) [[unlikely]] {
            Error err("no renderer for this platform");
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        // Save additional macros that were specified.
        pCreatedPipeline->additionalVertexShaderMacros = additionalVertexShaderMacros;
        pCreatedPipeline->additionalPixelShaderMacros = additionalPixelShaderMacros;

        return pCreatedPipeline;
    }

    std::variant<std::shared_ptr<Pipeline>, Error> Pipeline::createComputePipeline(
        Renderer* pRenderer, PipelineManager* pPipelineManager, const std::string& sComputeShaderName) {
        std::shared_ptr<Pipeline> pCreatedPipeline = nullptr;
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            // Create DirectX PSO.
            auto result = DirectXPso::createComputePso(pRenderer, pPipelineManager, sComputeShaderName);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            return std::dynamic_pointer_cast<Pipeline>(std::get<std::shared_ptr<DirectXPso>>(result));
        }
#endif

        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            // Create Vulkan pipeline.
            auto result =
                VulkanPipeline::createComputePipeline(pRenderer, pPipelineManager, sComputeShaderName);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }

            return std::dynamic_pointer_cast<Pipeline>(std::get<std::shared_ptr<VulkanPipeline>>(result));
        }

        Error err("unsupported renderer");
        err.showError();
        throw std::runtime_error(err.getFullErrorMessage());

        return pCreatedPipeline;
    }

    std::string Pipeline::getPipelineIdentifier() const {
        if (!sComputeShaderName.empty()) {
            return sComputeShaderName;
        }
        return combineShaderNames(sVertexShaderName, sPixelShaderName);
    }

    std::set<ShaderMacro> Pipeline::getAdditionalVertexShaderMacros() const {
        return additionalVertexShaderMacros;
    }

    std::set<ShaderMacro> Pipeline::getAdditionalPixelShaderMacros() const {
        return additionalPixelShaderMacros;
    }

    void Pipeline::onMaterialUsingPipeline(Material* pMaterial) {
        std::scoped_lock guard(mtxMaterialsThatUseThisPipeline.first);

        // Self check: make sure this material was not added previously.
        const auto it = mtxMaterialsThatUseThisPipeline.second.find(pMaterial);
        if (it != mtxMaterialsThatUseThisPipeline.second.end()) [[unlikely]] {
            Logger::get().error(fmt::format(
                "material \"{}\" notified the pipeline for shaders \"{}\" of being used but this "
                "material already existed in the array of materials that use this pipeline",
                pMaterial->getMaterialName(),
                getPipelineIdentifier()));
            return;
        }

        // Add new material.
        mtxMaterialsThatUseThisPipeline.second.insert(pMaterial);

        // No need to notify the pipeline manager here.
    }

    void Pipeline::onMaterialNoLongerUsingPipeline(Material* pMaterial) {
        {
            // Use a nested scope in order to avoid mutex destroyed while locked (see below).
            std::scoped_lock guard(mtxMaterialsThatUseThisPipeline.first);

            // Make sure this material was previously added to our array of materials.
            const auto it = mtxMaterialsThatUseThisPipeline.second.find(pMaterial);
            if (it == mtxMaterialsThatUseThisPipeline.second.end()) [[unlikely]] {
                Logger::get().error(fmt::format(
                    "material \"{}\" notified the pipeline for shaders \"{}\" of no longer being used but "
                    "this "
                    "material was not found in the array of materials that use this pipeline",
                    pMaterial->getMaterialName(),
                    getPipelineIdentifier()));
                return;
            }

            // Remove material.
            mtxMaterialsThatUseThisPipeline.second.erase(it);
        }

        // Notify manager (this call might cause this object to be deleted thus we used a
        // nested scope for mutex).
        pPipelineManager->onPipelineNoLongerUsedByMaterial(getPipelineIdentifier());
    }

    void Pipeline::onComputeShaderUsingPipeline(ComputeShaderInterface* pComputeShaderInterface) {
        std::scoped_lock guard(mtxComputeShadersThatUseThisPipeline.first);

        // Self check: make sure this compute shader interface was not added previously.
        const auto it = mtxComputeShadersThatUseThisPipeline.second.find(pComputeShaderInterface);
        if (it != mtxComputeShadersThatUseThisPipeline.second.end()) [[unlikely]] {
            Logger::get().error(fmt::format(
                "a compute shader interface has started referencing the pipeline for shader \"{}\" but "
                "this compute interface already existed in the array of interfaces that use this pipeline",
                getPipelineIdentifier()));
            return;
        }

        // Add new interface.
        mtxComputeShadersThatUseThisPipeline.second.insert(pComputeShaderInterface);

        // No need to notify the pipeline manager here.
    }

    void Pipeline::onComputeShaderNoLongerUsingPipeline(ComputeShaderInterface* pComputeShaderInterface) {
        {
            // Use a nested scope in order to avoid mutex destroyed while locked (see below).
            std::scoped_lock guard(mtxComputeShadersThatUseThisPipeline.first);

            // Make sure this compute shader interface was previously added to our array of materials.
            const auto it = mtxComputeShadersThatUseThisPipeline.second.find(pComputeShaderInterface);
            if (it == mtxComputeShadersThatUseThisPipeline.second.end()) [[unlikely]] {
                Logger::get().error(fmt::format(
                    "a compute shader interface stopped referencing the pipeline for shader \"{}\" but "
                    "this compute interface was not found in the array of interfaces that use this pipeline",
                    getPipelineIdentifier()));
                return;
            }

            // Remove interface.
            mtxComputeShadersThatUseThisPipeline.second.erase(it);
        }

        // Notify manager (this call might cause this object to be deleted thus we used a
        // nested scope for mutex).
        pPipelineManager->onPipelineNoLongerUsedByComputeShaderInterface(
            sComputeShaderName, pComputeShaderInterface);
    }

    Renderer* Pipeline::getRenderer() const { return pRenderer; }

} // namespace ne
