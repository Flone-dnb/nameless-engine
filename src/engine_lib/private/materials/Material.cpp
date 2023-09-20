#include "materials/Material.h"

// Standard.
#include <format>

// Custom.
#include "game/GameManager.h"
#include "game/Window.h"
#include "io/Logger.h"
#include "game/nodes/MeshNode.h"
#include "materials/EngineShaderNames.hpp"

#include "Material.generated_impl.h"

/** The total number of alive material objects. */
static std::atomic<size_t> iTotalAliveMaterialCount{0};

namespace ne {

    Material::Material() {
        iTotalAliveMaterialCount.fetch_add(1);

        // Self check: make sure reinterpret_casts in `draw` will work.
        static_assert(
            std::is_same_v<
                decltype(mtxGpuResources.second.shaderResources.shaderCpuWriteResources),
                std::unordered_map<std::string, ShaderCpuWriteResourceUniquePtr>>,
            "update reinterpret_casts in both renderers (in draw function)");
        static_assert(
            std::is_same_v<
                decltype(mtxGpuResources.second.shaderResources.shaderTextureResources),
                std::unordered_map<std::string, ShaderTextureResourceUniquePtr>>,
            "update reinterpret_casts in both renderers (in draw function)");

#if defined(WIN32) && defined(DEBUG)
        static_assert(
            sizeof(GpuResources::ShaderResources) == 160, // NOLINT
            "add new static asserts for resources here");
#elif defined(DEBUG)
        static_assert(
            sizeof(GpuResources::ShaderResources) == 56, // NOLINT
            "add new static asserts for resources here");
#endif
    }

    Material::Material(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUseTransparency,
        PipelineManager* pPipelineManager,
        const std::string& sMaterialName)
        : Material() {
        this->sVertexShaderName = sVertexShaderName;
        this->sPixelShaderName = sPixelShaderName;
        this->bUseTransparency = bUseTransparency;
        this->pPipelineManager = pPipelineManager;
        this->sMaterialName = sMaterialName;

        if (bUseTransparency) {
            mtxInternalResources.second.materialPixelShaderMacros.insert(
                ShaderMacro::PS_USE_MATERIAL_TRANSPARENCY);
        }
    }

    Material::~Material() {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first, mtxInternalResources.first);

        // Make sure there are no nodes that reference this material.
        auto iMeshNodeCount = mtxSpawnedMeshNodesThatUseThisMaterial.second.getTotalSize();
        if (iMeshNodeCount != 0) [[unlikely]] {
            Logger::get().error(std::format(
                "material \"{}\" is being destroyed but material's array of spawned mesh nodes "
                "that use this material still has {} item(s)",
                sMaterialName,
                iMeshNodeCount));
        }

        // Make sure pipeline was cleared.
        if (mtxInternalResources.second.pUsedPipeline.isInitialized()) [[unlikely]] {
            Logger::get().error(std::format(
                "material \"{}\" is being destroyed but used pipeline was not cleared", sMaterialName));
            mtxInternalResources.second.pUsedPipeline.clear();
        }

        // Make sure shader resources were deallocated.
        if (bIsShaderResourcesAllocated) [[unlikely]] {
            Logger::get().error(std::format(
                "material \"{}\" is being destroyed but shader resources were not deallocated",
                sMaterialName));
        }

        // Update total material count.
        iTotalAliveMaterialCount.fetch_sub(1);
    }

    size_t Material::getCurrentAliveMaterialCount() { return iTotalAliveMaterialCount.load(); }

    void Material::onSpawnedMeshNodeRecreatedIndexBuffer(
        MeshNode* pMeshNode,
        const std::pair<GpuResource*, unsigned int>& deletedIndexBuffer,
        const std::pair<GpuResource*, unsigned int>& newIndexBuffer) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first, mtxInternalResources.first);

        // Depending on the node's visibility pick a map to modify.
        std::unordered_map<MeshNode*, std::vector<MeshIndexBufferInfo>>* pNodeMap = nullptr;
        if (pMeshNode->isVisible()) {
            pNodeMap = &mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes;
        } else {
            pNodeMap = &mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes;
        }

        // Find this mesh node.
        const auto nodeIt = pNodeMap->find(pMeshNode);
        if (nodeIt == pNodeMap->end()) [[unlikely]] {
            Error error(std::format(
                "spawned mesh node \"{}\" notified the material \"{}\" about re-created index buffer "
                "but this material is not displaying any index buffer of this mesh",
                pMeshNode->getNodeName(),
                getMaterialName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Find and replace old index buffer.
        bool bReplaced = false;
        for (size_t i = 0; i < nodeIt->second.size(); i++) {
            if (nodeIt->second[i].pIndexBuffer != deletedIndexBuffer.first) {
                continue;
            }

            bReplaced = true;
            nodeIt->second[i] = MeshIndexBufferInfo(newIndexBuffer.first, newIndexBuffer.second);
            break;
        }

        // Make sure we replaced the index buffer.
        if (!bReplaced) [[unlikely]] {
            Error error(std::format(
                "spawned mesh node \"{}\" notified the material \"{}\" about re-created index buffer "
                "but although this material is displaying some index buffer(s) of this mesh the material "
                "was unable to find the specified deleted index buffer to replace it",
                pMeshNode->getNodeName(),
                getMaterialName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    void Material::onMeshNodeSpawning(
        MeshNode* pMeshNode, const std::pair<GpuResource*, unsigned int>& indexBufferToDisplay) {
        onSpawnedMeshNodeStartedUsingMaterial(pMeshNode, indexBufferToDisplay);
    }

    void Material::onSpawnedMeshNodeStartedUsingMaterial(
        MeshNode* pMeshNode, const std::pair<GpuResource*, unsigned int>& indexBufferToDisplay) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first, mtxInternalResources.first);

        // Depending on the node's visibility pick a map to modify.
        std::unordered_map<MeshNode*, std::vector<MeshIndexBufferInfo>>* pNodeMap = nullptr;
        if (pMeshNode->isVisible()) {
            pNodeMap = &mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes;
        } else {
            pNodeMap = &mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes;
        }

        // See if this mesh node is already displaying some index buffer using this material.
        const auto it = pNodeMap->find(pMeshNode);
        if (it == pNodeMap->end()) {
            // Add node to be considered.
            pNodeMap->operator[](pMeshNode) = {
                MeshIndexBufferInfo(indexBufferToDisplay.first, indexBufferToDisplay.second)};
        } else {
            // Some index buffer of this mesh is already displayed using this material.
            // Make sure this index buffer is not registered yet.
            for (const auto& indexBufferInfo : it->second) {
                if (indexBufferInfo.pIndexBuffer == indexBufferToDisplay.first) [[unlikely]] {
                    Error error(std::format(
                        "spawned mesh node \"{}\" notified the material \"{}\" about using it to display an "
                        "index buffer but this index buffer is already displayed by this material",
                        pMeshNode->getNodeName(),
                        getMaterialName()));
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }
            }

            // Add this new index buffer to be considered.
            it->second.push_back(
                MeshIndexBufferInfo(indexBufferToDisplay.first, indexBufferToDisplay.second));
        }

        // Initialize pipeline if needed.
        if (!mtxInternalResources.second.pUsedPipeline.isInitialized()) {
            auto result = pPipelineManager->getGraphicsPipelineForMaterial(
                sVertexShaderName,
                sPixelShaderName,
                bUseTransparency,
                mtxInternalResources.second.materialVertexShaderMacros,
                mtxInternalResources.second.materialPixelShaderMacros,
                this);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
            mtxInternalResources.second.pUsedPipeline = std::get<PipelineSharedPtr>(std::move(result));

            allocateShaderResources();
        }
    }

    void Material::onSpawnedMeshNodeStoppedUsingMaterial(
        MeshNode* pMeshNode, const std::pair<GpuResource*, unsigned int>& indexBufferDisplayed) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first, mtxInternalResources.first);

        // Depending on the node's visibility pick a map to modify.
        std::unordered_map<MeshNode*, std::vector<MeshIndexBufferInfo>>* pNodeMap = nullptr;
        if (pMeshNode->isVisible()) {
            pNodeMap = &mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes;
        } else {
            pNodeMap = &mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes;
        }

        // Find this mesh node in our map.
        const auto nodeIt = pNodeMap->find(pMeshNode);
        if (nodeIt == pNodeMap->end()) [[unlikely]] {
            Error error(std::format(
                "spawned mesh node \"{}\" notified the material \"{}\" about no longer using this material "
                "to display some index buffer but this material is not displaying any index "
                "buffer of this mesh",
                pMeshNode->getNodeName(),
                getMaterialName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Find this index buffer.
        bool bFound = false;
        for (auto it = nodeIt->second.begin(); it != nodeIt->second.end(); ++it) {
            if (it->pIndexBuffer != indexBufferDisplayed.first) {
                continue;
            }

            bFound = true;
            nodeIt->second.erase(it);
            break;
        }
        if (!bFound) [[unlikely]] {
            Error error(std::format(
                "spawned mesh node \"{}\" notified the material \"{}\" about no longer using this material "
                "to display some index buffer but although this material is displaying some index "
                "buffer(s) of this mesh the material was unable to find the specified index buffer",
                pMeshNode->getNodeName(),
                getMaterialName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // See if index buffer array is now empty.
        if (!nodeIt->second.empty()) {
            // Nothing more to do.
            return;
        }

        // Remove this node from consideration.
        pNodeMap->erase(nodeIt);

        // Check if need to free pipeline.
        if (mtxSpawnedMeshNodesThatUseThisMaterial.second.getTotalSize() == 0) {
            // Self check: make sure our pipeline is initialized.
            if (!mtxInternalResources.second.pUsedPipeline.isInitialized()) [[unlikely]] {
                Error error(std::format(
                    "no mesh is now referencing the material \"{}\" but material's pipeline pointer was not "
                    "initialized previously",
                    sMaterialName));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Deallocate all resources first (because they reference things from pipeline).
            deallocateShaderResources();

            // Don't reference our pipeline anymore since we don't need it
            // (so it may be freed).
            mtxInternalResources.second.pUsedPipeline.clear();
        }
    }

    void Material::onMeshNodeDespawning(
        MeshNode* pMeshNode, const std::pair<GpuResource*, unsigned int>& indexBufferDisplayed) {
        onSpawnedMeshNodeStoppedUsingMaterial(pMeshNode, indexBufferDisplayed);
    }

    std::variant<std::shared_ptr<Material>, Error> Material::create(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUseTransparency,
        const std::string& sMaterialName) {
        // Get pipeline manager.
        auto result = getPipelineManagerForNewMaterial(sVertexShaderName, sPixelShaderName);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto pPipelineManager = std::get<PipelineManager*>(result);

        // Create material.
        return std::shared_ptr<Material>(new Material(
            sVertexShaderName, sPixelShaderName, bUseTransparency, pPipelineManager, sMaterialName));
    }

    std::pair<std::mutex, MeshNodesThatUseThisMaterial>* Material::getSpawnedMeshNodesThatUseThisMaterial() {
        return &mtxSpawnedMeshNodesThatUseThisMaterial;
    }

    std::string Material::getMaterialName() const { return sMaterialName; }

    bool Material::isUsingTransparency() const { return bUseTransparency; }

    void Material::onSpawnedMeshNodeChangedVisibility(MeshNode* pMeshNode, bool bOldVisibility) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        // Make sure the visibility actually changed.
        if (bOldVisibility == pMeshNode->isVisible()) [[unlikely]] {
            Logger::get().error(std::format(
                "mesh node \"{}\" notified used material about changed visibility but the visibility "
                "of this mesh node was not changed",
                pMeshNode->getNodeName()));
            return;
        }

        if (bOldVisibility) {
            // Became invisible.
            // Find in visible nodes.
            const auto it = mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.find(pMeshNode);
            if (it == mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.end()) [[unlikely]] {
                Logger::get().error(std::format(
                    "mesh node \"{}\" notified used material about changed visibility but this mesh node "
                    "does not exist in material's array of spawned mesh nodes",
                    pMeshNode->getNodeName()));
                return;
            }

            // Save index buffers to display.
            auto vIndexBuffersToDisplay = std::move(it->second);

            // Remove from visible.
            mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.erase(it);

            // Add to invisible nodes.
            mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes[pMeshNode] =
                std::move(vIndexBuffersToDisplay);
        } else {
            // Became visible.
            // Find in invisible nodes.
            const auto it = mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.find(pMeshNode);
            if (it == mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.end()) [[unlikely]] {
                Logger::get().error(std::format(
                    "mesh node \"{}\" notified used material about changed visibility but this mesh node "
                    "does not exist in material's array of spawned mesh nodes",
                    pMeshNode->getNodeName()));
                return;
            }

            // Save index buffers to display.
            auto vIndexBuffersToDisplay = std::move(it->second);

            // Remove from invisible nodes.
            mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.erase(it);

            // Add to visible nodes.
            mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes[pMeshNode] =
                std::move(vIndexBuffersToDisplay);
        }
    }

    Pipeline* Material::getUsedPipeline() const {
        return mtxInternalResources.second.pUsedPipeline.getPipeline();
    }

    void Material::allocateShaderResources() {
        std::scoped_lock guard(mtxInternalResources.first, mtxGpuResources.first);

        // Make sure resources were not allocated before.
        if (bIsShaderResourcesAllocated) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" was requested to allocate shader resources but shader resources already "
                "allocated",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure pipeline is initialized.
        if (!mtxInternalResources.second.pUsedPipeline.isInitialized()) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" was requested to allocate shader resources but pipeline is not initialized",
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

        // Set diffuse texture (if path is set).
        if (!sDiffuseTexturePathRelativeRes.empty()) {
            setShaderTextureResourceBindingData(
                sMaterialShaderDiffuseTextureName, sDiffuseTexturePathRelativeRes);
        }
    }

    void Material::deallocateShaderResources() {
        std::scoped_lock guard(mtxInternalResources.first, mtxGpuResources.first);

        // Make sure shader resources were previously allocated.
        if (!bIsShaderResourcesAllocated) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" was requested to deallocate shader resources but shader resources were not "
                "allocated yet",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure pipeline is initialized.
        if (!mtxInternalResources.second.pUsedPipeline.isInitialized()) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" was requested to deallocate shader resources but pipeline is not "
                "initialized",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure the GPU is not using our resources.
        const auto pRenderer = pPipelineManager->getRenderer();
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

        // Make sure shader resources are allocated.
        if (!bIsShaderResourcesAllocated) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" requested to set shader resource binding data but shader "
                "resources were not allocated yet",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure pipeline is initialized.
        if (!mtxInternalResources.second.pUsedPipeline.isInitialized()) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" requested to set shader resources binding data but pipeline is not "
                "initialized",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure there is no resource with this name.
        auto it = mtxGpuResources.second.shaderResources.shaderCpuWriteResources.find(sShaderResourceName);
        if (it != mtxGpuResources.second.shaderResources.shaderCpuWriteResources.end()) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" already has a shader CPU write resource with the name \"{}\"",
                sMaterialName,
                sShaderResourceName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Create object data constant buffer for shaders.
        const auto pShaderWriteResourceManager =
            pPipelineManager->getRenderer()->getShaderCpuWriteResourceManager();
        auto result = pShaderWriteResourceManager->createShaderCpuWriteResource(
            sShaderResourceName,
            std::format("material \"{}\"", sMaterialName),
            iResourceSizeInBytes,
            {mtxInternalResources.second.pUsedPipeline.getPipeline()},
            onStartedUpdatingResource,
            onFinishedUpdatingResource);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Add to be considered.
        mtxGpuResources.second.shaderResources.shaderCpuWriteResources[sShaderResourceName] =
            std::get<ShaderCpuWriteResourceUniquePtr>(std::move(result));
    }

    void Material::setShaderTextureResourceBindingData(
        const std::string& sShaderResourceName, const std::string& sPathToTextureResourceRelativeRes) {
        std::scoped_lock guard(mtxInternalResources.first, mtxGpuResources.first);

        // Make sure shader resources are allocated.
        if (!bIsShaderResourcesAllocated) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" requested to set shader resource binding data but shader "
                "resources were not allocated yet",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure pipeline is initialized.
        if (!mtxInternalResources.second.pUsedPipeline.isInitialized()) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" requested to set shader resources binding data but pipeline is not "
                "initialized",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure there is no resource with this name.
        auto it = mtxGpuResources.second.shaderResources.shaderTextureResources.find(sShaderResourceName);
        if (it != mtxGpuResources.second.shaderResources.shaderTextureResources.end()) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" already has a shader texture resource with the name \"{}\"",
                sMaterialName,
                sShaderResourceName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get texture manager.
        const auto pRenderer = mtxInternalResources.second.pUsedPipeline->getRenderer();
        const auto pTextureManager = pRenderer->getResourceManager()->getTextureManager();

        // Get texture handle.
        auto textureResult = pTextureManager->getTexture(sPathToTextureResourceRelativeRes);
        if (std::holds_alternative<Error>(textureResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(textureResult));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        auto pTextureHandle = std::get<std::unique_ptr<TextureHandle>>(std::move(textureResult));

        // Get shader texture resource manager.
        const auto pShaderResourceManager = pRenderer->getShaderTextureResourceManager();

        // Create a new shader resource.
        auto shaderResourceResult = pShaderResourceManager->createShaderTextureResource(
            sShaderResourceName,
            std::format("material \"{}\"", sMaterialName),
            {mtxInternalResources.second.pUsedPipeline.getPipeline()},
            std::move(pTextureHandle));
        if (std::holds_alternative<Error>(shaderResourceResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(shaderResourceResult));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Add to be considered.
        mtxGpuResources.second.shaderResources.shaderTextureResources[sShaderResourceName] =
            std::get<ShaderTextureResourceUniquePtr>(std::move(shaderResourceResult));
    }

    void Material::markShaderCpuWriteResourceAsNeedsUpdate(const std::string& sShaderResourceName) {
        std::scoped_lock guard(mtxInternalResources.first, mtxGpuResources.first);

        // Make sure shader resources allocated.
        if (!bIsShaderResourcesAllocated) {
            return; // silently exit, this is not an error
        }

        // Make sure pipeline is initialized.
        if (!mtxInternalResources.second.pUsedPipeline.isInitialized()) {
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
            Logger::get().error(std::format(
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

    void Material::updateToNewPipeline() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Self check: make sure current pipeline is initialized.
        if (!mtxInternalResources.second.pUsedPipeline.isInitialized()) [[unlikely]] {
            Error error(
                std::format("expected the pipeline to be initialized on material \"{}\"", sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get renderer.
        const auto pRenderer = mtxInternalResources.second.pUsedPipeline->getRenderer();

        // Make sure no rendering happens during the following process
        // (since we might delete some resources and also want to avoid deleting resources in the
        // middle of the `draw` function, any resource deletion will cause this thread to wait for
        // renderer to finish its current `draw` function which might create deadlocks if not called
        // right now).
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Deallocate shader resources before deinitializing our pipeline because
        // shader resources reference pipeline resources.
        deallocateShaderResources();

        // Don't reference the current pipeline anymore. This might cause the pipeline to be
        // destroyed.
        mtxInternalResources.second.pUsedPipeline.clear();

        // Get us a new pipeline.
        auto result = pPipelineManager->getGraphicsPipelineForMaterial(
            sVertexShaderName,
            sPixelShaderName,
            bUseTransparency,
            mtxInternalResources.second.materialVertexShaderMacros,
            mtxInternalResources.second.materialPixelShaderMacros,
            this);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        mtxInternalResources.second.pUsedPipeline = std::get<PipelineSharedPtr>(std::move(result));

        // Create our shader resources.
        allocateShaderResources();

        // Notify mesh nodes about changed pipeline.
        std::scoped_lock nodesGuard(mtxSpawnedMeshNodesThatUseThisMaterial.first);
        for (const auto& [pVisibleMeshNode, vIndexBuffersToDisplay] :
             mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes) {
            pVisibleMeshNode->updateShaderResourcesToUseChangedMaterialPipelines();
        }
        for (const auto& [pInvisibleMeshNode, vIndexBuffersToDisplay] :
             mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes) {
            pInvisibleMeshNode->updateShaderResourcesToUseChangedMaterialPipelines();
        }
#if defined(WIN32) && defined(DEBUG)
        static_assert(sizeof(MeshNodesThatUseThisMaterial) == 160, "notify new nodes here");
#elif defined(DEBUG)
        static_assert(sizeof(MeshNodesThatUseThisMaterial) == 96, "notify new nodes here");
#endif
    }

    void Material::setEnableTransparency(bool bEnable) {
        std::scoped_lock internalResourcesGuard(mtxInternalResources.first);

        if (bUseTransparency == bEnable) {
            // Nothing to do.
            return;
        }

        // Update our macros.
        auto& pixelShaderMacros = mtxInternalResources.second.materialPixelShaderMacros;
        if (bEnable) {
            pixelShaderMacros.insert(ShaderMacro::PS_USE_MATERIAL_TRANSPARENCY);
        } else {
            pixelShaderMacros.erase(ShaderMacro::PS_USE_MATERIAL_TRANSPARENCY);
        }

        // Save new transparency option.
        bUseTransparency = bEnable;

        // See if we need to re-request a pipeline.
        if (!mtxInternalResources.second.pUsedPipeline.isInitialized()) {
            // No more things to do here.
            return;
        }

        // Get renderer.
        const auto pRenderer = mtxInternalResources.second.pUsedPipeline->getRenderer();

        // Make sure no rendering happens during the following process
        // (since we might delete some resources and also want to avoid deleting resources in the
        // middle of the `draw` function, any resource deletion will cause this thread to wait for
        // renderer to finish its current `draw` function which might create deadlocks if not called
        // right now).
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        updateToNewPipeline();
    }

    void Material::setDiffuseTexture(const std::string& sTextureResourcePathRelativeRes) {
        // See if this path is not equal to the current one.
        const bool bPathToTextureResourceDifferent =
            sTextureResourcePathRelativeRes != sDiffuseTexturePathRelativeRes;
        if (!bPathToTextureResourceDifferent) {
            // Nothing to do.
            return;
        }

        // We can lock internal resources mutex before waiting for GPU to finish its work (see below,
        // we make the renderer wait in some cases) because we don't use this mutex inside of our `draw`
        // function - locking this mutex won't cause a deadlock).
        std::scoped_lock internalResourcesGuard(mtxInternalResources.first);

        // Set new diffuse texture path.
        sDiffuseTexturePathRelativeRes = sTextureResourcePathRelativeRes;

        // Update our macros.
        auto& pixelShaderMacros = mtxInternalResources.second.materialPixelShaderMacros;
        const auto diffuseTextureMacroIt = pixelShaderMacros.find(ShaderMacro::PS_USE_DIFFUSE_TEXTURE);

        bool bNeedNewPipeline = false;

        if (!sTextureResourcePathRelativeRes.empty() && diffuseTextureMacroIt == pixelShaderMacros.end()) {
            // Our current pipeline does not use diffuse textures, we need a new pipeline that
            // uses diffuse textures.

            // Add diffuse texture macro to our pixel/fragment shader macros.
            pixelShaderMacros.insert(ShaderMacro::PS_USE_DIFFUSE_TEXTURE);

            // Mark that we need a new pipeline.
            bNeedNewPipeline = true;
        } else if (
            sTextureResourcePathRelativeRes.empty() && diffuseTextureMacroIt != pixelShaderMacros.end()) {
            // Our current pipeline uses diffuse textures but we no longer need that, we need
            // a new pipeline that does not use diffuse textures.

            // Remove diffuse texture macro from our pixel/fragment shader macros.
            pixelShaderMacros.erase(diffuseTextureMacroIt);

            // Mark that we need a new pipeline.
            bNeedNewPipeline = true;
        }

        // See if we need to re-request a pipeline.
        if (!mtxInternalResources.second.pUsedPipeline.isInitialized()) {
            // No more things to do here.
            return;
        }

        // Get renderer.
        const auto pRenderer = mtxInternalResources.second.pUsedPipeline->getRenderer();

        // Make sure no rendering happens during the following process
        // (since we might delete some resources and also want to avoid deleting resources in the
        // middle of the `draw` function, any resource deletion will cause this thread to wait for
        // renderer to finish its current `draw` function which might create deadlocks if not called
        // right now).
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        if (bNeedNewPipeline) {
            updateToNewPipeline();
            return;
        }

        // Just change diffuse texture view.
        std::scoped_lock guard(mtxGpuResources.first);

        // Find a resource that handles diffuse textures.
        auto& resources = mtxGpuResources.second.shaderResources.shaderTextureResources;
        const auto it = resources.find(sMaterialShaderDiffuseTextureName);
        if (it == resources.end()) [[unlikely]] {
            // Some other thread changed GPU resources?
            Logger::get().error(
                std::format("expected shader resource \"{}\" to exist", sMaterialShaderDiffuseTextureName));
            return;
        }

        // Get texture handle.
        const auto pTextureManager = pRenderer->getResourceManager()->getTextureManager();
        auto textureResult = pTextureManager->getTexture(sDiffuseTexturePathRelativeRes);
        if (std::holds_alternative<Error>(textureResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(textureResult));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        auto pTextureHandle = std::get<std::unique_ptr<TextureHandle>>(std::move(textureResult));

        // Set new texture to be used.
        auto optionalError = it->second.getResource()->useNewTexture(std::move(pTextureHandle));
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }
    }

    glm::vec3 Material::getDiffuseColor() {
        std::scoped_lock guard(mtxShaderMaterialDataConstants.first);

        return mtxShaderMaterialDataConstants.second.diffuseColor;
    }

    float Material::getOpacity() {
        std::scoped_lock guard(mtxShaderMaterialDataConstants.first);

        return mtxShaderMaterialDataConstants.second.opacity;
    }

    std::string Material::getPathToDiffuseTextureResource() {
        std::scoped_lock internalResourcesGuard(mtxInternalResources.first);

        return sDiffuseTexturePathRelativeRes;
    }

    void Material::onAfterDeserialized() {
        Serializable::onAfterDeserialized();

        std::scoped_lock internalResourcesGuard(mtxInternalResources.first);

        // Save pipeline manager.
        auto result = getPipelineManagerForNewMaterial(sVertexShaderName, sPixelShaderName);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        pPipelineManager = std::get<PipelineManager*>(result);

        // Define diffuse texture macro (if enabled).
        if (!sDiffuseTexturePathRelativeRes.empty()) {
            mtxInternalResources.second.materialPixelShaderMacros.insert(ShaderMacro::PS_USE_DIFFUSE_TEXTURE);
        }

        // Define transparency macro (if enabled).
        if (bUseTransparency) {
            mtxInternalResources.second.materialPixelShaderMacros.insert(
                ShaderMacro::PS_USE_MATERIAL_TRANSPARENCY);
        }

#if defined(WIN32) && defined(DEBUG)
        static_assert(sizeof(Material) == 1008, "consider checking new macros here"); // NOLINT: current size
#elif defined(DEBUG)
        static_assert(sizeof(Material) == 736, "consider checking new macros here"); // NOLINT: current size
#endif
    }

    bool Material::isTransparencyEnabled() {
        std::scoped_lock internalResourcesGuard(mtxInternalResources.first);

        return bUseTransparency;
    }

    std::variant<PipelineManager*, Error> Material::getPipelineManagerForNewMaterial(
        const std::string& sVertexShaderName, const std::string& sPixelShaderName) {
        // Get shader manager.
        const auto pGame = GameManager::get();
        if (pGame == nullptr) [[unlikely]] {
            return Error("unable to create material when game object is not created");
        }

        // Make sure the game is not being destroyed.
        if (pGame->isBeingDestroyed()) [[unlikely]] {
            return Error("unable to create material when game object is being destroyed");
        }

        // Get renderer.
        const auto pRenderer = pGame->getWindow()->getRenderer();
        if (pRenderer == nullptr) {
            return Error("unable to create material when renderer is not created");
        }

        const auto pShaderManager = pRenderer->getShaderManager();

        // Check that the specified shaders exist.
        // Check vertex shader.
        if (pShaderManager->isShaderNameCanBeUsed(sVertexShaderName)) {
            return Error(
                std::format("vertex shader \"{}\" was not found in the shader manager", sVertexShaderName));
        }

        // Check pixel shader.
        if (pShaderManager->isShaderNameCanBeUsed(sPixelShaderName)) {
            return Error(
                std::format("pixel shader \"{}\" was not found in the shader manager", sPixelShaderName));
        }

        return pRenderer->getPipelineManager();
    }

} // namespace ne
