#include "Pso.h"

// Custom.
#include "render/Renderer.h"
#include "io/Logger.h"
#include "render/general/pso/PsoManager.h"
#include "materials/Material.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "render/directx/pso/DirectXPso.h"
#endif

namespace ne {

    Pso::Pso(
        Renderer* pRenderer,
        PsoManager* pPsoManager,
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending)
        : ShaderUser(pRenderer->getShaderManager()) {
        this->pRenderer = pRenderer;
        this->pPsoManager = pPsoManager;

        this->sVertexShaderName = sVertexShaderName;
        this->sPixelShaderName = sPixelShaderName;

        bIsUsingPixelBlending = bUsePixelBlending;

        sUniquePsoIdentifier =
            constructUniquePsoIdentifier(sVertexShaderName, sPixelShaderName, bUsePixelBlending);
    }

    void Pso::saveUsedShaderConfiguration(ShaderType shaderType, std::set<ShaderMacro>&& fullConfiguration) {
        usedShaderConfiguration[shaderType] = std::move(fullConfiguration);
    }

    Pso::~Pso() {
        // Make sure the renderer is no longer using this PSO or its resources.
        Logger::get().info(
            "PSO is being destroyed, flushing the command queue before being deleted", sPsoLogCategory);
        getRenderer()->waitForGpuToFinishWorkUpToThisPoint();
    }

    std::string Pso::constructUniquePsoIdentifier(
        const std::string& sVertexShaderName, const std::string& sPixelShaderName, bool bUsePixelBlending) {
        // Prepare name.
        std::string sUniqueId = sVertexShaderName + "|" + sPixelShaderName;
        if (bUsePixelBlending) {
            sUniqueId += "(transparent)";
        }

        return sUniqueId;
    }

    std::string Pso::getVertexShaderName() { return sVertexShaderName; }

    std::string Pso::getPixelShaderName() { return sPixelShaderName; }

    std::optional<std::set<ShaderMacro>> Pso::getCurrentShaderConfiguration(ShaderType shaderType) {
        auto it = usedShaderConfiguration.find(shaderType);
        if (it == usedShaderConfiguration.end()) {
            return {};
        }

        return it->second;
    }

    bool Pso::isUsingPixelBlending() const { return bIsUsingPixelBlending; }

    std::pair<std::mutex, std::set<Material*>>* Pso::getMaterialsThatUseThisPso() {
        return &mtxMaterialsThatUseThisPso;
    }

    std::variant<std::shared_ptr<Pso>, Error> Pso::createGraphicsPso(
        Renderer* pRenderer,
        PsoManager* pPsoManager,
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUsePixelBlending,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::set<ShaderMacro>& additionalPixelShaderMacros) {
#if defined(WIN32)
        if (dynamic_cast<DirectXRenderer*>(pRenderer) != nullptr) {
            auto result = DirectXPso::createGraphicsPso(
                pRenderer,
                pPsoManager,
                sVertexShaderName,
                sPixelShaderName,
                bUsePixelBlending,
                additionalVertexShaderMacros,
                additionalPixelShaderMacros);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                return error;
            }

            return std::dynamic_pointer_cast<Pso>(std::get<std::shared_ptr<DirectXPso>>(result));
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

    std::string Pso::getUniquePsoIdentifier() const { return sUniquePsoIdentifier; }

    void Pso::onMaterialUsingPso(Material* pMaterial) {
        {
            std::scoped_lock guard(mtxMaterialsThatUseThisPso.first);

            // Check if this material was already added previously.
            const auto it = mtxMaterialsThatUseThisPso.second.find(pMaterial);
            if (it != mtxMaterialsThatUseThisPso.second.end()) [[unlikely]] {
                Logger::get().error(
                    fmt::format(
                        "material \"{}\" notified the PSO with ID \"{}\" of being used but this "
                        "material already existed in the array of materials that use this PSO",
                        pMaterial->getMaterialName(),
                        sUniquePsoIdentifier),
                    sPsoLogCategory);
                return;
            }

            mtxMaterialsThatUseThisPso.second.insert(pMaterial);
        }
    }

    void Pso::onMaterialNoLongerUsingPso(Material* pMaterial) {
        {
            std::scoped_lock guard(mtxMaterialsThatUseThisPso.first);

            // Make sure this material was previously added to our array of materials.
            const auto it = mtxMaterialsThatUseThisPso.second.find(pMaterial);
            if (it == mtxMaterialsThatUseThisPso.second.end()) [[unlikely]] {
                Logger::get().error(
                    fmt::format(
                        "material \"{}\" notified the PSO with ID \"{}\" of no longer being used but this "
                        "material was not found in the array of materials that use this PSO",
                        pMaterial->getMaterialName(),
                        sUniquePsoIdentifier),
                    sPsoLogCategory);
                return;
            }
        }

        pPsoManager->onPsoNoLongerUsedByMaterial(sUniquePsoIdentifier);
    }

    Renderer* Pso::getRenderer() const { return pRenderer; }

} // namespace ne
