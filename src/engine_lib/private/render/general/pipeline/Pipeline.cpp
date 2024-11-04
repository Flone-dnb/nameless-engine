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
        std::unique_ptr<PipelineConfiguration> pPipelineConfiguration)
        : ShaderUser(pRenderer->getShaderManager()),
          pPipelineConfiguration(std::move(pPipelineConfiguration)), pPipelineManager(pPipelineManager),
          pRenderer(pRenderer) {}

    void Pipeline::setShaderConstants(const std::unordered_map<std::string, size_t>& uintConstantsOffsets) {
        std::scoped_lock guard(mtxShaderConstantsData.first);

        // Clear constants data if empty.
        if (uintConstantsOffsets.empty()) {
            mtxShaderConstantsData.second = {};
            return;
        }

        // Prepare new data.
        ShaderConstantsData data;
        data.uintConstantsOffsets = uintConstantsOffsets;
        data.pConstantsManager =
            std::make_unique<PipelineShaderConstantsManager>(uintConstantsOffsets.size());

        // Initialize.
        mtxShaderConstantsData.second = std::move(data);
    }

    std::string Pipeline::combineShaderNames(
        std::string_view sVertexShaderName,
        std::string_view sPixelShaderName,
        std::string_view sComputeShaderName) {
        if (!sComputeShaderName.empty()) {
            return std::string(sComputeShaderName);
        }

        if (sPixelShaderName.empty()) {
            return std::string(sVertexShaderName);
        }

        return std::format("{} / {}", sVertexShaderName, sPixelShaderName);
    }

    std::variant<size_t, Error> Pipeline::getUintConstantOffset(const std::string& sConstantName) {
        // Lock both.
        std::scoped_lock guard(mtxShaderConstantsData.first);

        // Make sure root/push constants are used.
        if (!mtxShaderConstantsData.second.has_value()) [[unlikely]] {
            return Error(std::format(
                "expected root/push constants to be used on the pipeline \"{}\" because tried to find a "
                "constant named \"{}\"",
                getPipelineIdentifier(),
                sConstantName));
        }

        const auto& uintConstantsOffsets = mtxShaderConstantsData.second->uintConstantsOffsets;

        // Make sure that name of this field exists in shader code.
        const auto fieldNameIt = uintConstantsOffsets.find(sConstantName);
        if (fieldNameIt == uintConstantsOffsets.end()) [[unlikely]] {
            return Error(std::format(
                "expected to find the field named \"{}\" in root/push constants (in shader code), pipeline: "
                "{}",
                sConstantName,
                getPipelineIdentifier()));
        }

        return fieldNameIt->second;
    }

    std::pair<std::mutex, std::unordered_set<Material*>>* Pipeline::getMaterialsThatUseThisPipeline() {
        return &mtxMaterialsThatUseThisPipeline;
    }

    std::variant<std::shared_ptr<Pipeline>, Error> Pipeline::createGraphicsPipeline(
        Renderer* pRenderer,
        PipelineManager* pPipelineManager,
        std::unique_ptr<PipelineConfiguration> pPipelineConfiguration) {
        // Prepare resulting pipeline pointer.
        std::shared_ptr<Pipeline> pCreatedPipeline = nullptr;

#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            // Create DirectX PSO.
            auto result =
                DirectXPso::createGraphicsPso(pRenderer, pPipelineManager, std::move(pPipelineConfiguration));
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
                pRenderer, pPipelineManager, std::move(pPipelineConfiguration));
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
        return combineShaderNames(
            pPipelineConfiguration->getVertexShaderName(),
            pPipelineConfiguration->getFragmentShaderName(),
            pPipelineConfiguration->getComputeShaderName());
    }

    std::pair<std::mutex, std::optional<Pipeline::ShaderConstantsData>>* Pipeline::getShaderConstants() {
        return &mtxShaderConstantsData;
    }

    void Pipeline::onMaterialUsingPipeline(Material* pMaterial) {
        std::scoped_lock guard(mtxMaterialsThatUseThisPipeline.first);

        // Self check: make sure this material was not added previously.
        const auto it = mtxMaterialsThatUseThisPipeline.second.find(pMaterial);
        if (it != mtxMaterialsThatUseThisPipeline.second.end()) [[unlikely]] {
            Logger::get().error(std::format(
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
                Logger::get().error(std::format(
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
            Logger::get().error(std::format(
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
                Logger::get().error(std::format(
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
            std::string(pPipelineConfiguration->getComputeShaderName()), pComputeShaderInterface);
    }

    Renderer* Pipeline::getRenderer() const { return pRenderer; }

    const PipelineConfiguration* Pipeline::getConfiguration() const { return pPipelineConfiguration.get(); }

    void Pipeline::ShaderConstantsData::findOffsetAndCopySpecialValueToConstant(
        Pipeline* pPipeline, const char* pConstantName, unsigned int iValueToCopy) {
        // Get offset of the constant.
        const auto indexIt = uintConstantsOffsets.find(pConstantName);

#if defined(DEBUG)
        // Make sure it's valid.
        if (indexIt == uintConstantsOffsets.end()) [[unlikely]] {
            Error error(std::format(
                "expected shader constant \"{}\" to be used on pipeline \"{}\"",
                pConstantName,
                pPipeline->getPipelineIdentifier()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
#endif

        // Copy to constants.
        pConstantsManager->copyValueToShaderConstant(indexIt->second, iValueToCopy);
    }

} // namespace ne
