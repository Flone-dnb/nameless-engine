#include "Pipeline.h"

// Custom.
#include "render/Renderer.h"
#include "io/Logger.h"
#include "render/general/pipeline/PipelineManager.h"
#include "materials/Material.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "render/directx/pipeline/DirectXPso.h"
#endif

namespace ne {

    Pipeline::Pipeline(
        Renderer* pRenderer,
        PipelineManager* pPipelineManager,
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending)
        : ShaderUser(pRenderer->getShaderManager()) {
        this->pRenderer = pRenderer;
        this->pPipelineManager = pPipelineManager;

        this->sVertexShaderName = sVertexShaderName;
        this->sPixelShaderName = sPixelShaderName;

        bIsUsingPixelBlending = bUsePixelBlending;

        sUniquePipelineIdentifier =
            constructUniquePipelineIdentifier(sVertexShaderName, sPixelShaderName, bUsePixelBlending);
    }

    void
    Pipeline::saveUsedShaderConfiguration(ShaderType shaderType, std::set<ShaderMacro>&& fullConfiguration) {
        usedShaderConfiguration[shaderType] = std::move(fullConfiguration);
    }

    Pipeline::~Pipeline() {
        // Make sure the renderer is no longer using this PSO or its resources.
        Logger::get().info("Pipeline is being destroyed, waiting for the GPU to finish work up to this point "
                           "before being deleted");
        getRenderer()->waitForGpuToFinishWorkUpToThisPoint();
    }

    std::string Pipeline::constructUniquePipelineIdentifier(
        const std::string& sVertexShaderName, const std::string& sPixelShaderName, bool bUsePixelBlending) {
        // Prepare name.
        std::string sUniqueId = sVertexShaderName + "|" + sPixelShaderName;
        if (bUsePixelBlending) {
            sUniqueId += "(transparent)";
        }

        return sUniqueId;
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

    std::pair<std::mutex, std::set<Material*>>* Pipeline::getMaterialsThatUseThisPipeline() {
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
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
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

            return std::dynamic_pointer_cast<Pipeline>(std::get<std::shared_ptr<DirectXPso>>(result));
        }
#elif __linux__

        static_assert(false, "not implemented");
        if (dynamic_cast<VulkanRenderer*>(pRenderer) != nullptr) {
            // TODO
        }
#endif

        Error err("no renderer for this platform");
        err.showError();
        throw std::runtime_error(err.getFullErrorMessage());
    }

    std::string Pipeline::getUniquePipelineIdentifier() const { return sUniquePipelineIdentifier; }

    void Pipeline::onMaterialUsingPipeline(Material* pMaterial) {
        {
            std::scoped_lock guard(mtxMaterialsThatUseThisPipeline.first);

            // Check if this material was already added previously.
            const auto it = mtxMaterialsThatUseThisPipeline.second.find(pMaterial);
            if (it != mtxMaterialsThatUseThisPipeline.second.end()) [[unlikely]] {
                Logger::get().error(fmt::format(
                    "material \"{}\" notified the pipeline with ID \"{}\" of being used but this "
                    "material already existed in the array of materials that use this pipeline",
                    pMaterial->getMaterialName(),
                    sUniquePipelineIdentifier));
                return;
            }

            mtxMaterialsThatUseThisPipeline.second.insert(pMaterial);
        }
    }

    void Pipeline::onMaterialNoLongerUsingPipeline(Material* pMaterial) {
        {
            std::scoped_lock guard(mtxMaterialsThatUseThisPipeline.first);

            // Make sure this material was previously added to our array of materials.
            const auto it = mtxMaterialsThatUseThisPipeline.second.find(pMaterial);
            if (it == mtxMaterialsThatUseThisPipeline.second.end()) [[unlikely]] {
                Logger::get().error(fmt::format(
                    "material \"{}\" notified the pipeline with ID \"{}\" of no longer being used but this "
                    "material was not found in the array of materials that use this pipeline",
                    pMaterial->getMaterialName(),
                    sUniquePipelineIdentifier));
                return;
            }

            mtxMaterialsThatUseThisPipeline.second.erase(it);
        }

        pPipelineManager->onPipelineNoLongerUsedByMaterial(sUniquePipelineIdentifier);
    }

    Renderer* Pipeline::getRenderer() const { return pRenderer; }

} // namespace ne
