#include "materials/Material.h"

// Custom.
#include "game/GameManager.h"
#include "game/Window.h"
#include "io/Logger.h"
#include "game/nodes/MeshNode.h"
#include "materials/EngineShaderNames.hpp"

#include "Material.generated_impl.h"

/** Total amount of created materials. */
static std::atomic<size_t> iTotalMaterialCount{0};

namespace ne {

    Material::Material() { iTotalMaterialCount.fetch_add(1); }

    Material::Material(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUseTransparency,
        PsoManager* pPsoManager,
        const std::string& sMaterialName)
        : Material() {
        this->sVertexShaderName = sVertexShaderName;
        this->sPixelShaderName = sPixelShaderName;
        this->bUseTransparency = bUseTransparency;
        this->pPsoManager = pPsoManager;
        this->sMaterialName = sMaterialName;

        if (bUseTransparency) {
            mtxInternalResources.second.materialPixelShaderMacros.insert(
                ShaderMacro::USE_MATERIAL_TRANSPARENCY);
        }
    }

    Material::~Material() {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first, mtxInternalResources.first);

        // Make sure there are no nodes that reference this material.
        auto iMeshNodeCount = mtxSpawnedMeshNodesThatUseThisMaterial.second.getTotalSize();
        if (iMeshNodeCount != 0) [[unlikely]] {
            Logger::get().error(fmt::format(
                "material \"{}\" is being destroyed but material's array of spawned mesh nodes "
                "that use this material still has {} item(s)",
                sMaterialName,
                iMeshNodeCount));
        }

        // Make sure PSO was cleared.
        if (mtxInternalResources.second.pUsedPso.isInitialized()) [[unlikely]] {
            Logger::get().error(fmt::format(
                "material \"{}\" is being destroyed but used PSO was not cleared", sMaterialName));
            mtxInternalResources.second.pUsedPso.clear();
        }

        // Make sure shader resources were deallocated.
        if (bIsShaderResourcesAllocated) [[unlikely]] {
            Logger::get().error(fmt::format(
                "material \"{}\" is being destroyed but shader resources were not deallocated",
                sMaterialName));
        }

        // Update total material count.
        iTotalMaterialCount.fetch_sub(1);
    }

    size_t Material::getCurrentMaterialCount() { return iTotalMaterialCount.load(); }

    void Material::onMeshNodeSpawned(MeshNode* pMeshNode) {
        onSpawnedMeshNodeStartedUsingMaterial(pMeshNode);
    }

    void Material::onSpawnedMeshNodeStartedUsingMaterial(MeshNode* pMeshNode) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first, mtxInternalResources.first);

        // Make sure we don't have this mesh node in our array.
        if (mtxSpawnedMeshNodesThatUseThisMaterial.second.isMeshNodeAdded(pMeshNode)) [[unlikely]] {
            Logger::get().error(fmt::format(
                "mesh node \"{}\" notified used material about being spawned but this mesh node already "
                "exists in material's array of spawned mesh nodes",
                pMeshNode->getNodeName()));
            return;
        }

        // Add node to be considered.
        if (pMeshNode->isVisible()) {
            mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.insert(pMeshNode);
        } else {
            mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.insert(pMeshNode);
        }

        // Initialize PSO if needed.
        if (!mtxInternalResources.second.pUsedPso.isInitialized()) {
            auto result = pPsoManager->getGraphicsPsoForMaterial(
                sVertexShaderName,
                sPixelShaderName,
                bUseTransparency,
                mtxInternalResources.second.materialVertexShaderMacros,
                mtxInternalResources.second.materialPixelShaderMacros,
                this);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
            mtxInternalResources.second.pUsedPso = std::get<PsoSharedPtr>(std::move(result));

            allocateShaderResources();
        }
    }

    void Material::onSpawnedMeshNodeStoppedUsingMaterial(MeshNode* pMeshNode) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first, mtxInternalResources.first);

        // Make sure we have this mesh node in our array.
        if (!mtxSpawnedMeshNodesThatUseThisMaterial.second.isMeshNodeAdded(pMeshNode)) [[unlikely]] {
            Logger::get().error(fmt::format(
                "mesh node \"{}\" notified used material about no longer being used but this mesh node "
                "does not exist in material's array of spawned mesh nodes",
                pMeshNode->getNodeName()));
            return;
        }

        // Remove node from consideration.
        if (pMeshNode->isVisible()) {
            mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.erase(pMeshNode);
        } else {
            mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.erase(pMeshNode);
        }

        // Check if need to free PSO.
        if (mtxSpawnedMeshNodesThatUseThisMaterial.second.getTotalSize() == 0) {
            deallocateShaderResources();
            mtxInternalResources.second.pUsedPso.clear();
        }
    }

    void Material::onMeshNodeDespawned(MeshNode* pMeshNode) {
        onSpawnedMeshNodeStoppedUsingMaterial(pMeshNode);
    }

    std::variant<std::shared_ptr<Material>, Error> Material::create(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUseTransparency,
        const std::string& sMaterialName) {
        // Get shader manager.
        const auto pGame = GameManager::get();
        if (pGame == nullptr) [[unlikely]] {
            return Error("unable to create material when game object is not created");
        }

        if (pGame->isBeingDestroyed()) [[unlikely]] {
            return Error("unable to create material when game object is being destroyed");
        }

        const auto pRenderer = pGame->getWindow()->getRenderer();
        if (pRenderer == nullptr) {
            return Error("unable to create material when renderer is not created");
        }

        const auto pShaderManager = pRenderer->getShaderManager();

        // Check that the specified shaders exist.
        // Check vertex shader.
        if (pShaderManager->isShaderNameCanBeUsed(sVertexShaderName)) {
            return Error(
                fmt::format("vertex shader \"{}\" was not found in the shader manager", sVertexShaderName));
        }

        // Check pixel shader.
        if (pShaderManager->isShaderNameCanBeUsed(sPixelShaderName)) {
            return Error(
                fmt::format("pixel shader \"{}\" was not found in the shader manager", sPixelShaderName));
        }

        // Create material.
        return std::shared_ptr<Material>(new Material(
            sVertexShaderName,
            sPixelShaderName,
            bUseTransparency,
            pRenderer->getPsoManager(),
            sMaterialName));
    }

    std::pair<std::mutex, MeshNodesThatUseThisMaterial>* Material::getSpawnedMeshNodesThatUseThisMaterial() {
        return &mtxSpawnedMeshNodesThatUseThisMaterial;
    }

    std::string Material::getMaterialName() const { return sMaterialName; }

    bool Material::isUsingTransparency() const { return bUseTransparency; }

    void Material::onSpawnedMeshNodeChangedVisibility(MeshNode* pMeshNode, bool bOldVisibility) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        if (bOldVisibility == pMeshNode->isVisible()) [[unlikely]] {
            Logger::get().error(fmt::format(
                "mesh node \"{}\" notified used material about changed visibility but the visibility "
                "of this mesh node was not changed",
                pMeshNode->getNodeName()));
            return;
        }

        if (bOldVisibility) {
            auto it = mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.find(pMeshNode);
            if (it == mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.end()) [[unlikely]] {
                Logger::get().error(fmt::format(
                    "mesh node \"{}\" notified used material about changed visibility but this mesh node "
                    "does not exist in material's array of spawned mesh nodes",
                    pMeshNode->getNodeName()));
                return;
            }
            mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.erase(it);
            mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.insert(pMeshNode);
        } else {
            auto it = mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.find(pMeshNode);
            if (it == mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.end()) [[unlikely]] {
                Logger::get().error(fmt::format(
                    "mesh node \"{}\" notified used material about changed visibility but this mesh node "
                    "does not exist in material's array of spawned mesh nodes",
                    pMeshNode->getNodeName()));
                return;
            }
            mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.erase(it);
            mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.insert(pMeshNode);
        }
    }

    Pso* Material::getUsedPso() const { return mtxInternalResources.second.pUsedPso.getPso(); }

    void Material::allocateShaderResources() {
        std::scoped_lock guard(mtxInternalResources.first, mtxGpuResources.first);

        // Make sure resources were not allocated before.
        if (bIsShaderResourcesAllocated) [[unlikely]] {
            Error error(fmt::format(
                "material \"{}\" was requested to allocate shader resources but shader resources already "
                "allocated",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure PSO is initialized.
        if (!mtxInternalResources.second.pUsedPso.isInitialized()) [[unlikely]] {
            Error error(fmt::format(
                "material \"{}\" was requested to allocate shader resources but PSO is not initialized",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Set flag before adding binding data.
        bIsShaderResourcesAllocated = true;

        // Set material constant buffer.
        setShaderCpuWriteResourceBindingData(
            sMaterialShaderConstantBufferName,
            sizeof(MaterialShaderConstants),
            [this]() -> void* { return onStartUpdatingShaderMeshConstants(); },
            [this]() { return onFinishedUpdatingShaderMeshConstants(); });
    }

    void Material::deallocateShaderResources() {
        std::scoped_lock guard(mtxInternalResources.first, mtxGpuResources.first);

        // Make sure shader resources were previously allocated.
        if (!bIsShaderResourcesAllocated) [[unlikely]] {
            Error error(fmt::format(
                "material \"{}\" was requested to deallocate shader resources but shader resources were not "
                "allocated yet",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure PSO is initialized.
        if (!mtxInternalResources.second.pUsedPso.isInitialized()) [[unlikely]] {
            Error error(fmt::format(
                "material \"{}\" was requested to deallocate shader resources but PSO is not initialized",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure the GPU is not using our resources.
        const auto pRenderer = pPsoManager->getRenderer();
        std::scoped_lock renderGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Clear flag.
        bIsShaderResourcesAllocated = false;

        // Deallocate resources.
        mtxGpuResources.second.shaderResources = {};
    }

    void Material::setShaderCpuWriteResourceBindingData(
        const std::string& sShaderResourceName,
        size_t iResourceSizeInBytes,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        std::scoped_lock guard(mtxInternalResources.first, mtxGpuResources.first);

        // Make sure shader resources allocated.
        if (!bIsShaderResourcesAllocated) [[unlikely]] {
            Error error(fmt::format(
                "material \"{}\" was requested to set shader CPU write resource binding data all shader "
                "resources were not allocated yet",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure PSO is initialized.
        if (!mtxInternalResources.second.pUsedPso.isInitialized()) [[unlikely]] {
            Error error(fmt::format(
                "material \"{}\" was requested to allocate shader resources but PSO is not initialized",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure there is no resource with this name.
        auto it = mtxGpuResources.second.shaderResources.shaderCpuWriteResources.find(sShaderResourceName);
        if (it != mtxGpuResources.second.shaderResources.shaderCpuWriteResources.end()) [[unlikely]] {
            Error error(fmt::format(
                "material \"{}\" already has a shader CPU write resource with the name \"{}\"",
                sMaterialName,
                sShaderResourceName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Create object data constant buffer for shaders.
        const auto pShaderWriteResourceManager =
            pPsoManager->getRenderer()->getShaderCpuWriteResourceManager();
        auto result = pShaderWriteResourceManager->createShaderCpuWriteResource(
            sShaderResourceName,
            fmt::format("material \"{}\"", sMaterialName),
            iResourceSizeInBytes,
            mtxInternalResources.second.pUsedPso.getPso(),
            onStartedUpdatingResource,
            onFinishedUpdatingResource);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Add to be considered.
        mtxGpuResources.second.shaderResources.shaderCpuWriteResources[sShaderResourceName] =
            std::get<ShaderCpuWriteResourceUniquePtr>(std::move(result));
    }

    void Material::markShaderCpuWriteResourceAsNeedsUpdate(const std::string& sShaderResourceName) {
        std::scoped_lock guard(mtxInternalResources.first, mtxGpuResources.first);

        // Make sure shader resources allocated.
        if (!bIsShaderResourcesAllocated) {
            return; // silently exit, this is not an error
        }

        // Make sure PSO is initialized.
        if (!mtxInternalResources.second.pUsedPso.isInitialized()) {
            return; // silently exit, this is not an error
        }

        // Make sure there is a resource with this name.
        auto it = mtxGpuResources.second.shaderResources.shaderCpuWriteResources.find(sShaderResourceName);
        if (it == mtxGpuResources.second.shaderResources.shaderCpuWriteResources.end()) {
            return; // silently exit, this is not an error
        }

        // Mark as needs update.
        it->second.markAsNeedsUpdate();
    }

    void* Material::onStartUpdatingShaderMeshConstants() {
        mtxShaderMaterialDataConstants.first.lock();
        return &mtxShaderMaterialDataConstants.second;
    }

    void Material::onFinishedUpdatingShaderMeshConstants() { mtxShaderMaterialDataConstants.first.unlock(); }

    void Material::setDiffuseColor(const glm::vec3& diffuseColor) {
        std::scoped_lock guard(mtxShaderMaterialDataConstants.first);

        mtxShaderMaterialDataConstants.second.diffuseColor = diffuseColor;

        markShaderCpuWriteResourceAsNeedsUpdate(sMaterialShaderConstantBufferName);
    }

    void Material::setOpacity(float opacity) {
        if (!bUseTransparency) [[unlikely]] {
            Logger::get().error(fmt::format(
                "material \"{}\" was requested to set opacity but it does not use transparency",
                sMaterialName));
            return;
        }

        std::scoped_lock guard(mtxShaderMaterialDataConstants.first);

        mtxShaderMaterialDataConstants.second.opacity = std::clamp(opacity, 0.0F, 1.0F);

        markShaderCpuWriteResourceAsNeedsUpdate(sMaterialShaderConstantBufferName);
    }

    std::string Material::getVertexShaderName() const { return sVertexShaderName; }

    std::string Material::getPixelShaderName() const { return sPixelShaderName; }

} // namespace ne
