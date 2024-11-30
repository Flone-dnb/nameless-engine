#include "material/Material.h"

// Standard.
#include <format>

// Custom.
#include "game/GameManager.h"
#include "game/Window.h"
#include "io/Logger.h"
#include "game/node/MeshNode.h"
#include "render/general/resource/GpuResourceManager.h"
#include "shader/general/resource/binding/cpuwrite/ShaderCpuWriteResourceBindingManager.h"
#include "shader/general/resource/binding/texture/ShaderTextureResourceBindingManager.h"
#include "shader/ShaderManager.h"
#include "render/general/pipeline/PipelineManager.h"
#include "material/TextureManager.h"

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
                std::unordered_map<std::string, ShaderCpuWriteResourceBindingUniquePtr>>,
            "update reinterpret_casts in both renderers (in draw function)");
        static_assert(
            std::is_same_v<
                decltype(mtxGpuResources.second.shaderResources.shaderTextureResources),
                std::unordered_map<std::string, ShaderTextureResourceBindingUniquePtr>>,
            "update reinterpret_casts in both renderers (in draw function)");

#if defined(WIN32) && defined(DEBUG)
        static_assert(
            sizeof(GpuResources::ShaderResources) == 160, // NOLINT
            "add new static asserts for resources here");
#elif defined(DEBUG)
        static_assert(
            sizeof(GpuResources::ShaderResources) == 112, // NOLINT
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
        if (mtxInternalResources.second.pColorPipeline.isInitialized()) [[unlikely]] {
            Error error("expected pipeline to be deinitialized at this point");
            error.showError();
            return; // don't throw in destructor
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
        if (!mtxInternalResources.second.pColorPipeline.isInitialized()) {
            // Initialize pipeline.
            auto optionalError = initializePipelines();
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                optionalError->showError();
                throw std::runtime_error(optionalError->getFullErrorMessage());
            }

            // Initialize shader resources.
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
            if (!mtxInternalResources.second.pColorPipeline.isInitialized()) [[unlikely]] {
                Error error(std::format(
                    "no mesh is now referencing the material \"{}\" but material's pipeline pointer was not "
                    "initialized previously",
                    sMaterialName));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Deallocate all resources first (because they reference things from pipeline).
            deallocateShaderResources();

            // Don't reference our pipelines anymore since we don't need them (so they may be freed).
            resetPipelines();
        }
    }

    void Material::onMeshNodeDespawning(
        MeshNode* pMeshNode, const std::pair<GpuResource*, unsigned int>& indexBufferDisplayed) {
        onSpawnedMeshNodeStoppedUsingMaterial(pMeshNode, indexBufferDisplayed);
    }

    std::variant<std::unique_ptr<Material>, Error> Material::create(
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
        return std::unique_ptr<Material>(new Material(
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

    Pipeline* Material::getColorPipeline() const {
        return mtxInternalResources.second.pColorPipeline.getPipeline();
    }

    Pipeline* Material::getDepthOnlyPipeline() const {
        return mtxInternalResources.second.pDepthOnlyPipeline.getPipeline();
    }

    Pipeline* Material::getShadowMappingDirectionalSpotPipeline() const {
        return mtxInternalResources.second.pShadowMappingDirectionalSpotPipeline.getPipeline();
    }

    Pipeline* Material::getShadowMappingPointPipeline() const {
        return mtxInternalResources.second.pShadowMappingPointPipeline.getPipeline();
    }

    void Material::allocateShaderResources() {
        std::scoped_lock guard(
            mtxInternalResources.first, mtxGpuResources.first, mtxShaderMaterialDataConstants.first);

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
        if (!mtxInternalResources.second.pColorPipeline.isInitialized()) [[unlikely]] {
            Error error(std::format(
                "material \"{}\" was requested to allocate shader resources but pipeline is not initialized",
                sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Copy up to date data to shaders.
        mtxShaderMaterialDataConstants.second.diffuseColor = glm::vec4(diffuseColor, opacity);
        mtxShaderMaterialDataConstants.second.roughness = roughness;
        mtxShaderMaterialDataConstants.second.specularColor = glm::vec4(specularColor, 0.0F);
#if defined(WIN32) && defined(DEBUG)
        static_assert(
            sizeof(MaterialShaderConstants) == 48,
            "consider copying new data to shaders here"); // NOLINT: current size
#endif

        // Set flag before adding binding data.
        bIsShaderResourcesAllocated = true;

        // Set material data binding.
        setShaderCpuWriteResourceBinding(
            sMaterialShaderBufferName,
            sizeof(MaterialShaderConstants),
            [this]() -> void* { return onStartUpdatingShaderMeshConstants(); },
            [this]() { onFinishedUpdatingShaderMeshConstants(); });

        // Set diffuse texture binding (if path is set).
        if (!sDiffuseTexturePathRelativeRes.empty()) {
            setShaderTextureResourceBinding(
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
        if (!mtxInternalResources.second.pColorPipeline.isInitialized()) [[unlikely]] {
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

    void Material::setShaderCpuWriteResourceBinding(
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
        if (!mtxInternalResources.second.pColorPipeline.isInitialized()) [[unlikely]] {
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
            {mtxInternalResources.second.pColorPipeline.getPipeline()},
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
            std::get<ShaderCpuWriteResourceBindingUniquePtr>(std::move(result));
    }

    void Material::setShaderTextureResourceBinding(
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
        if (!mtxInternalResources.second.pColorPipeline.isInitialized()) [[unlikely]] {
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
        const auto pRenderer = mtxInternalResources.second.pColorPipeline->getRenderer();
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
            {mtxInternalResources.second.pColorPipeline.getPipeline()},
            std::move(pTextureHandle));
        if (std::holds_alternative<Error>(shaderResourceResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(shaderResourceResult));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Add to be considered.
        mtxGpuResources.second.shaderResources.shaderTextureResources[sShaderResourceName] =
            std::get<ShaderTextureResourceBindingUniquePtr>(std::move(shaderResourceResult));
    }

    void Material::markShaderCpuWriteResourceAsNeedsUpdate(const std::string& sShaderResourceName) {
        std::scoped_lock guard(mtxInternalResources.first, mtxGpuResources.first);

        // Make sure shader resources allocated.
        if (!bIsShaderResourcesAllocated) {
            return; // silently exit, this is not an error
        }

        // Make sure pipeline is initialized.
        if (!mtxInternalResources.second.pColorPipeline.isInitialized()) {
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

        // Save new diffuse color (to serialize).
        this->diffuseColor = diffuseColor;

        // Save new diffuse color for shaders (4th component stores opacity).
        mtxShaderMaterialDataConstants.second.diffuseColor.x = diffuseColor.x;
        mtxShaderMaterialDataConstants.second.diffuseColor.y = diffuseColor.y;
        mtxShaderMaterialDataConstants.second.diffuseColor.z = diffuseColor.z;

        markShaderCpuWriteResourceAsNeedsUpdate(sMaterialShaderBufferName);
    }

    void Material::setSpecularColor(const glm::vec3& specularColor) {
        std::scoped_lock guard(mtxShaderMaterialDataConstants.first);

        // Save new specular color (to serialize).
        this->specularColor = specularColor;

        // Save new specular color for shaders (4th component is not used).
        mtxShaderMaterialDataConstants.second.specularColor.x = specularColor.x;
        mtxShaderMaterialDataConstants.second.specularColor.y = specularColor.y;
        mtxShaderMaterialDataConstants.second.specularColor.z = specularColor.z;

        markShaderCpuWriteResourceAsNeedsUpdate(sMaterialShaderBufferName);
    }

    void Material::setRoughness(float roughness) {
        std::scoped_lock guard(mtxShaderMaterialDataConstants.first);

        // Save new roughness (to serialize).
        this->roughness = std::clamp(roughness, 0.0F, 1.0F);

        // Save new opacity for shaders.
        mtxShaderMaterialDataConstants.second.roughness = this->roughness;

        markShaderCpuWriteResourceAsNeedsUpdate(sMaterialShaderBufferName);
    }

    void Material::setOpacity(float opacity) {
        std::scoped_lock guard(mtxShaderMaterialDataConstants.first);

        // Save new opacity (to serialize).
        this->opacity = std::clamp(opacity, 0.0F, 1.0F);

        // Save new opacity for shaders.
        mtxShaderMaterialDataConstants.second.diffuseColor.w = this->opacity;

        markShaderCpuWriteResourceAsNeedsUpdate(sMaterialShaderBufferName);
    }

    std::string Material::getVertexShaderName() const { return sVertexShaderName; }

    std::string Material::getPixelShaderName() const { return sPixelShaderName; }

    void Material::updateToNewPipeline() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Self check: make sure current pipeline is initialized.
        if (!mtxInternalResources.second.pColorPipeline.isInitialized()) [[unlikely]] {
            Error error(
                std::format("expected the pipeline to be initialized on material \"{}\"", sMaterialName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get renderer.
        const auto pRenderer = mtxInternalResources.second.pColorPipeline->getRenderer();

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

        // Don't reference the current pipeline anymore. This might cause the pipeline to be destroyed.
        resetPipelines();

        // Get us a new pipeline.
        auto optionalError = initializePipelines();
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }

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
        static_assert(sizeof(MeshNodesThatUseThisMaterial) == 112, "notify new nodes here");
#endif
    }

    void Material::setEnableTransparency(bool bEnable) {
        std::scoped_lock internalResourcesGuard(mtxInternalResources.first);

        if (bUseTransparency == bEnable) {
            // Nothing to do.
            return;
        }

        // Save new transparency option.
        bUseTransparency = bEnable;

        // See if we need to re-request a pipeline.
        if (!mtxInternalResources.second.pColorPipeline.isInitialized()) {
            // No more things to do here.
            return;
        }

        // Get renderer.
        const auto pRenderer = mtxInternalResources.second.pColorPipeline->getRenderer();

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

        // See if we need to maybe re-request a new color.
        if (!mtxInternalResources.second.pColorPipeline.isInitialized()) {
            // No more things to do here.
            return;
        }
        bool bNeedNewPipeline = false;

        // Get pipeline macros.
        const auto currentAdditionalPixelMacros = mtxInternalResources.second.pColorPipeline.getPipeline()
                                                      ->getConfiguration()
                                                      ->getRequiredFragmentShaderMacros();
        const auto bPipelineDefinedDiffuseTextureMacro =
            currentAdditionalPixelMacros.find(ShaderMacro::FS_USE_DIFFUSE_TEXTURE) !=
            currentAdditionalPixelMacros.end();

        if (!sTextureResourcePathRelativeRes.empty() && !bPipelineDefinedDiffuseTextureMacro) {
            // Our current pipeline does not use diffuse textures, we need a new pipeline that
            // uses diffuse textures.

            // Mark that we need a new pipeline.
            bNeedNewPipeline = true;
        } else if (sTextureResourcePathRelativeRes.empty() && bPipelineDefinedDiffuseTextureMacro) {
            // Our current pipeline uses diffuse textures but we no longer need that, we need
            // a new pipeline that does not use diffuse textures.

            // Mark that we need a new pipeline.
            bNeedNewPipeline = true;
        }

        // Get renderer.
        const auto pRenderer = mtxInternalResources.second.pColorPipeline->getRenderer();

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

    glm::vec3 Material::getDiffuseColor() const { return diffuseColor; }

    float Material::getOpacity() const { return opacity; }

    std::string Material::getPathToDiffuseTextureResource() {
        std::scoped_lock internalResourcesGuard(mtxInternalResources.first);

        return sDiffuseTexturePathRelativeRes;
    }

    void Material::onAfterDeserialized() {
        Serializable::onAfterDeserialized();

        std::scoped_lock internalResourcesGuard(mtxInternalResources.first);

        // Save pointer to pipeline manager.
        auto result = getPipelineManagerForNewMaterial(sVertexShaderName, sPixelShaderName);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        pPipelineManager = std::get<PipelineManager*>(result);

        // Apply deserialized values to shaders.
        setDiffuseColor(diffuseColor);
        setSpecularColor(specularColor);
        setOpacity(opacity);
        setRoughness(roughness);
#if defined(WIN32) && defined(DEBUG)
        static_assert(
            sizeof(MaterialShaderConstants) == 48,
            "consider copying new data to shaders here"); // NOLINT: current size
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

    float Material::getRoughness() const { return roughness; }

    glm::vec3 Material::getSpecularColor() const { return specularColor; }

    std::optional<Error> Material::initializePipelines() {
        std::scoped_lock guard(mtxInternalResources.first);

        // Make sure pipeline was not initialized yet.
        if (mtxInternalResources.second.pColorPipeline.isInitialized()) [[unlikely]] {
            return Error("pipeline is already initialized");
        }

        // Get macros that we need to be defined.
        const auto materialVertexMacros = getVertexShaderMacrosForCurrentState();
        const auto materialPixelMacros = getPixelShaderMacrosForCurrentState();

        // Get color pipeline.
        auto result = pPipelineManager->getGraphicsPipelineForMaterial(
            std::unique_ptr<ColorPipelineConfiguration>(new ColorPipelineConfiguration(
                sVertexShaderName,
                materialVertexMacros,
                sPixelShaderName,
                materialPixelMacros,
                bUseTransparency)),
            this);
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        mtxInternalResources.second.pColorPipeline = std::get<PipelineSharedPtr>(std::move(result));

        if (!bUseTransparency) {
            // Get depth only pipeline for depth prepass.
            result = pPipelineManager->getGraphicsPipelineForMaterial(
                std::unique_ptr<DepthPipelineConfiguration>(
                    new DepthPipelineConfiguration(sVertexShaderName, materialVertexMacros, {})),
                this);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            mtxInternalResources.second.pDepthOnlyPipeline = std::get<PipelineSharedPtr>(std::move(result));

            // Get shadow mapping pipeline for directional and spot lights.
            result = pPipelineManager->getGraphicsPipelineForMaterial(
                std::unique_ptr<DepthPipelineConfiguration>(new DepthPipelineConfiguration(
                    sVertexShaderName,
                    materialVertexMacros,
                    PipelineShadowMappingUsage::DIRECTIONAL_AND_SPOT_LIGHTS)),
                this);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            mtxInternalResources.second.pShadowMappingDirectionalSpotPipeline =
                std::get<PipelineSharedPtr>(std::move(result));

            // Get shadow mapping pipeline for point lights.
            result = pPipelineManager->getGraphicsPipelineForMaterial(
                std::unique_ptr<DepthPipelineConfiguration>(new DepthPipelineConfiguration(
                    sVertexShaderName, materialVertexMacros, PipelineShadowMappingUsage::POINT_LIGHTS)),
                this);
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            mtxInternalResources.second.pShadowMappingPointPipeline =
                std::get<PipelineSharedPtr>(std::move(result));
        }

#if defined(DEBUG)
        static_assert(sizeof(InternalResources) == 128, "consider adding new pipelines here"); // NOLINT
#endif

        return {};
    }

    void Material::resetPipelines() {
        std::scoped_lock guard(mtxInternalResources.first);

        // don't check if pipelines are initialized, some pipelines may be not initialized

        mtxInternalResources.second.pColorPipeline.clear();
        mtxInternalResources.second.pDepthOnlyPipeline.clear();
        mtxInternalResources.second.pShadowMappingDirectionalSpotPipeline.clear();
        mtxInternalResources.second.pShadowMappingPointPipeline.clear();

#if defined(DEBUG)
        static_assert(sizeof(InternalResources) == 128, "consider adding new pipelines here"); // NOLINT
#endif
    }

    std::set<ShaderMacro> Material::getPixelShaderMacrosForCurrentState() {
        std::set<ShaderMacro> pixelMacros;

        // Define diffuse texture macro (if texture is set).
        if (!sDiffuseTexturePathRelativeRes.empty()) {
            pixelMacros.insert(ShaderMacro::FS_USE_DIFFUSE_TEXTURE);
        }

        // Define transparency macro (if enabled).
        if (bUseTransparency) {
            pixelMacros.insert(ShaderMacro::FS_USE_MATERIAL_TRANSPARENCY);
        }

        return pixelMacros;
    }

    std::set<ShaderMacro>
    Material::getVertexShaderMacrosForCurrentState() { // NOLINT: should not be static (since it will later
                                                       // contain some logic)
        return {};
    }
} // namespace ne
