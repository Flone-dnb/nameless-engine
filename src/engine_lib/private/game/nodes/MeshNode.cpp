#include "game/nodes/MeshNode.h"

// Standard.
#include <format>

// Custom.
#include "material/Material.h"
#include "game/Window.h"
#include "render/Renderer.h"
#include "render/general/resources/GpuResourceManager.h"
#include "shader/general/resources/cpuwrite/ShaderCpuWriteResourceManager.h"
#include "shader/general/EngineShaderNames.hpp"
#include "misc/PrimitiveMeshGenerator.h"

#include "MeshNode.generated_impl.h"

namespace ne {

    MeshNode::MeshNode() : MeshNode("Mesh Node") {}

    MeshNode::MeshNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
        // Create default mesh.
        std::scoped_lock guard(mtxMeshData);
        meshData = PrimitiveMeshGenerator::createCube(1.0F);
        onMeshDataChanged();

        // Self check: make sure reinterpret_casts in `draw` will work.
        static_assert(
            std::is_same_v<
                decltype(mtxGpuResources.second.shaderResources.shaderCpuWriteResources),
                std::unordered_map<std::string, ShaderCpuWriteResourceUniquePtr>>,
            "update reinterpret_casts in both renderers (in draw function)");

#if defined(WIN32) && defined(DEBUG)
        static_assert(
            sizeof(GpuResources::ShaderResources) == 80, // NOLINT
            "add new static asserts for resources here");
#elif defined(DEBUG)
        static_assert(
            sizeof(GpuResources::ShaderResources) == 56, // NOLINT
            "add new static asserts for resources here");
#endif
    }

    void MeshNode::setMaterial(std::shared_ptr<Material> pMaterial, size_t iMaterialSlot) {
        // Make sure the specified material is not a `nullptr`.
        if (pMaterial == nullptr) [[unlikely]] {
            auto error = Error("cannot use `nullptr` as a material");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // See if the node is spawned or not and lock spawning mutex
        // (we can lock this mutex before waiting for GPU to finish its work because we don't
        // use this mutex inside of our `draw` function - locking this mutex won't cause a deadlock).
        std::scoped_lock spawnGuard(*getSpawnDespawnMutex());
        const auto bIsSpawned = isSpawned();

        // Make sure the specified material slot exists.
        if (iMaterialSlot >= vMaterials.size()) [[unlikely]] {
            Logger::get().error(std::format(
                "mesh node \"{}\" was requested to set a new material to slot {} but this mesh only has {} "
                "available slot(s)",
                getNodeName(),
                iMaterialSlot,
                vMaterials.size()));
            return;
        }

        // Get renderer.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();

        if (bIsSpawned) {
            // Make sure no rendering happens during the material changing process
            // (since we might delete some resources and also want to avoid deleting resources in the middle
            // of the `draw` function, any resource deletion will cause this thread to wait for renderer
            // to finish its current `draw` function which might create deadlocks if not called right now).
            pRenderer->getRenderResourcesMutex()->lock();
            pRenderer->waitForGpuToFinishWorkUpToThisPoint();

            // Get info about an index buffer that changes material.
            const auto indexBufferToDisplay = getIndexBufferInfoForMaterialSlot(iMaterialSlot);

            // Notify old material.
            vMaterials[iMaterialSlot]->onSpawnedMeshNodeStoppedUsingMaterial(this, indexBufferToDisplay);

            // Notify new material.
            pMaterial->onSpawnedMeshNodeStartedUsingMaterial(this, indexBufferToDisplay);
        }

        // Update used material.
        vMaterials[iMaterialSlot] = std::move(pMaterial);

        if (bIsSpawned) {
            updateShaderResourcesToUseChangedMaterialPipelines();
            pRenderer->getRenderResourcesMutex()->unlock();
        }
    }

    std::shared_ptr<Material> MeshNode::getMaterial(size_t iMaterialSlot) {
        std::scoped_lock guard(*getSpawnDespawnMutex());

        // Make sure the specified slot is valid.
        if (iMaterialSlot >= vMaterials.size()) [[unlikely]] {
            return nullptr;
        }

        return vMaterials[iMaterialSlot];
    }

    void MeshNode::onWorldLocationRotationScaleChanged() {
        SpatialNode::onWorldLocationRotationScaleChanged();

        // Update shader constants.
        std::scoped_lock guard(mtxShaderMeshDataConstants.first);
        mtxShaderMeshDataConstants.second.world = getWorldMatrix();
        mtxShaderMeshDataConstants.second.normal =
            glm::transpose(glm::inverse(mtxShaderMeshDataConstants.second.world));

        // Mark data to be copied to the GPU.
        markShaderCpuWriteResourceToBeCopiedToGpu(sMeshShaderConstantBufferName);
    }

    MeshData::MeshData() {
        // Due to the type we use to store our indices, we expect 32 bits for indices.
        // (due to used index format == `DXGI_FORMAT_R32_UINT`)
        constexpr size_t iExpectedSize = 4;
        constexpr auto iActualSize = sizeof(MeshData::meshindex_t);
        if (iActualSize != iExpectedSize) [[unlikely]] {
            Error error(std::format(
                "expected mesh index type to be {} bytes long, got: {}", iExpectedSize, iActualSize));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    bool MeshVertex::operator==(const MeshVertex& other) const {
        static_assert(
            sizeof(MeshVertex) == 32, // NOLINT
            "add new fields here");
        constexpr auto floatDelta = 0.00001F;
        return glm::all(glm::epsilonEqual(position, other.position, floatDelta)) &&
               glm::all(glm::epsilonEqual(normal, other.normal, floatDelta)) &&
               glm::all(glm::epsilonEqual(uv, other.uv, floatDelta));
    }

    void MeshNode::setMeshData(const MeshData& meshData) {
        std::scoped_lock guard(mtxMeshData);

        this->meshData = meshData;

        onMeshDataChanged();
    }

    void MeshNode::setMeshData(MeshData&& meshData) {
        std::scoped_lock guard(mtxMeshData);

        this->meshData = std::move(meshData);

        onMeshDataChanged();
    }

    void MeshNode::allocateShaderResources() {
        std::scoped_lock guard(*getSpawnDespawnMutex(), mtxGpuResources.first);

        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(std::format(
                "mesh node \"{}\" was requested to allocate shader resources but the node is not "
                "spawned",
                getNodeName()));
            return;
        }

        // Set object constant buffer.
        setShaderCpuWriteResourceBindingData(
            sMeshShaderConstantBufferName,
            sizeof(MeshShaderConstants),
            [this]() -> void* { return onStartedUpdatingShaderMeshConstants(); },
            [this]() { onFinishedUpdatingShaderMeshConstants(); });
    }

    void MeshNode::deallocateShaderResources() {
        std::scoped_lock guard(*getSpawnDespawnMutex(), mtxGpuResources.first);

        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(std::format(
                "mesh node \"{}\" was requested to deallocate shader resources but the node is "
                "not spawned",
                getNodeName()));
            return;
        }

        // Make sure the GPU is not using our resources.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
        std::scoped_lock renderGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Deallocate resources.
        mtxGpuResources.second.shaderResources = {};
    }

    void MeshNode::allocateGeometryBuffers() {
        std::scoped_lock guard(*getSpawnDespawnMutex(), mtxMeshData, mtxGpuResources.first);

        // Make sure we are spawned.
        if (!isSpawned()) [[unlikely]] {
            Logger::get().error(std::format(
                "mesh node \"{}\" was requested to allocate geometry buffers but the node is not "
                "spawned",
                getNodeName()));
            return;
        }

        // Make sure vertex/index buffers are not created.
        if (mtxGpuResources.second.mesh.pVertexBuffer != nullptr ||
            !mtxGpuResources.second.mesh.vIndexBuffers.empty()) [[unlikely]] {
            Logger::get().warn(std::format(
                "mesh node \"{}\" was requested to deallocate geometry buffers but they are already "
                "created",
                getNodeName()));
            return;
        }

        // Make sure we have vertices.
        if (meshData.getVertices()->empty()) [[unlikely]] {
            // This is a critical error because in the `draw` loop we will try to get the vertex
            // buffer but it will not exist.
            Error error(std::format("mesh node \"{}\" has no mesh vertices", getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure we have indices.
        const auto pIndices = meshData.getIndices();
        if (pIndices->empty()) [[unlikely]] {
            // This is a critical error because in the `draw` loop we will try to get the index
            // buffer but it will not exist.
            Error error(std::format("mesh node \"{}\" has no indices", getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        for (size_t i = 0; i < pIndices->size(); i++) {
            if (pIndices->at(i).empty()) [[unlikely]] {
                // This is a critical error because in the `draw` loop we will try to get the index
                // buffer but it will not exist.
                Error error(
                    std::format("mesh node \"{}\" material slot {} has no indices", getNodeName(), i));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
        }

        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
        const auto pResourceManager = pRenderer->getResourceManager();

        // Create vertex buffer.
        auto result = pResourceManager->createResourceWithData(
            std::format("mesh node \"{}\" vertex buffer", getNodeName()),
            meshData.getVertices()->data(),
            sizeof(MeshVertex),
            meshData.getVertices()->size(),
            ResourceUsageType::VERTEX_BUFFER,
            false);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        mtxGpuResources.second.mesh.pVertexBuffer = std::get<std::unique_ptr<GpuResource>>(std::move(result));

        // Create an index buffer per material slot.
        mtxGpuResources.second.mesh.vIndexBuffers.resize(pIndices->size());
        for (size_t i = 0; i < mtxGpuResources.second.mesh.vIndexBuffers.size(); i++) {
            result = pResourceManager->createResourceWithData(
                std::format("mesh node \"{}\" index buffer for material slot {}", getNodeName(), i),
                pIndices->at(i).data(),
                sizeof(MeshData::meshindex_t),
                pIndices->at(i).size(),
                ResourceUsageType::INDEX_BUFFER,
                false);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
            mtxGpuResources.second.mesh.vIndexBuffers[i] =
                std::get<std::unique_ptr<GpuResource>>(std::move(result));
        }
    }

    void MeshNode::deallocateGeometryBuffers() {
        std::scoped_lock guard(*getSpawnDespawnMutex(), mtxMeshData, mtxGpuResources.first);

        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(std::format(
                "mesh node \"{}\" was requested to deallocate geometry buffers but the node is not "
                "spawned",
                getNodeName()));
            return;
        }

        if (mtxGpuResources.second.mesh.pVertexBuffer == nullptr ||
            mtxGpuResources.second.mesh.vIndexBuffers.empty()) [[unlikely]] {
            Logger::get().warn(std::format(
                "mesh node \"{}\" was requested to deallocate geometry buffers but they were not "
                "created previously",
                getNodeName()));
            return;
        }

        // Make sure the GPU is not using our buffers.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
        std::scoped_lock renderGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Deallocate buffers.
        mtxGpuResources.second.mesh.pVertexBuffer = nullptr;
        mtxGpuResources.second.mesh.vIndexBuffers.clear();
    }

    void MeshNode::onMeshDataChanged() {
        std::scoped_lock guard(mtxMeshData, *getSpawnDespawnMutex(), mtxGpuResources.first);
        const auto bIsSpawned = isSpawned();

        const auto pIndices = meshData.getIndices();

        // Make sure there is at least one material slot.
        if (pIndices->empty()) [[unlikely]] {
            auto error = Error(std::format(
                "there are no indices in mesh \"{}\", at least one material slot is required",
                getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure all material slots have indices.
        for (size_t i = 0; i < pIndices->size(); i++) {
            if (pIndices->at(i).empty()) [[unlikely]] {
                auto error = Error(
                    std::format("there are no indices in mesh \"{}\" at material slot {}", getNodeName(), i));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
        }

        // Make sure we don't exceed type limit for vertices.
        const auto iVertexCount = meshData.getVertices()->size();
        if (iVertexCount > UINT_MAX) [[unlikely]] {
            // This will exceed type limit for DirectX vertex buffer view fields.
            auto error = Error(std::format(
                "the number of vertices in the mesh node \"{}\" has exceeded the maximum number of vertices "
                "(maximum is {}), can't continue because an overflow will occur",
                getNodeName(),
                UINT_MAX));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        const auto iVertexBufferSize = iVertexCount * sizeof(MeshVertex);
        if (iVertexBufferSize > UINT_MAX) [[unlikely]] {
            // This will exceed type limit for DirectX vertex buffer view fields.
            auto error = Error(std::format(
                "size of the vertex buffer ({} bytes) for the mesh node \"{}\" will exceed the limit of {} "
                "bytes ({} vertices * {} vertex size = {}), can't continue because an overflow will occur",
                iVertexBufferSize,
                getNodeName(),
                UINT_MAX,
                iVertexCount,
                sizeof(MeshVertex),
                iVertexBufferSize));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure we don't exceed type limit for indices.
        for (size_t i = 0; i < pIndices->size(); i++) {
            const auto iIndexCount = pIndices->at(i).size();
            const auto iIndexBufferSize = iIndexCount * sizeof(MeshData::meshindex_t);
            if (iIndexBufferSize > UINT_MAX) [[unlikely]] {
                // This will exceed type limit for DirectX vertex buffer view fields.
                auto error = Error(std::format(
                    "size of the index buffer ({} bytes) for the mesh node \"{}\" at material slot {} "
                    "will exceed the limit of {} bytes ({} indices * {} index size = {}), can't continue "
                    "because an overflow will occur",
                    iIndexBufferSize,
                    getNodeName(),
                    i,
                    UINT_MAX,
                    iIndexCount,
                    sizeof(MeshData::meshindex_t),
                    iIndexBufferSize));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
        }

        // Get renderer.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();

        // Prepare to update materials.
        const auto iNewMaterialSlotCount = pIndices->size();

        // Prepare an array of index buffers that will be deleted to pass pointers of deleted
        // index buffers to materials that we no longer use.
        std::vector<std::pair<GpuResource*, unsigned int>> vDeletedIndexBuffers;

        if (bIsSpawned) {
            // Make sure no rendering happens during the material changing process
            // (since we might delete some resources and also want to avoid deleting resources in the middle
            // of the `draw` function, any resource deletion will cause this thread to wait for renderer
            // to finish its current `draw` function which might create deadlocks if not called right now).
            pRenderer->getRenderResourcesMutex()->lock();
            pRenderer->waitForGpuToFinishWorkUpToThisPoint();

            // Gather index buffers that will be deleted.
            vDeletedIndexBuffers.resize(mtxGpuResources.second.mesh.vIndexBuffers.size());
            for (size_t i = 0; i < mtxGpuResources.second.mesh.vIndexBuffers.size(); i++) {
                vDeletedIndexBuffers[i] = {mtxGpuResources.second.mesh.vIndexBuffers[i].get(), 0};
            }

            // Re-create our vertex/index buffers and allocate an index buffer per material slot
            // before updating materials.
            deallocateGeometryBuffers();
            allocateGeometryBuffers();
        }

        // Now after index buffers were updated:
        // Update materials.
        if (vMaterials.size() > iNewMaterialSlotCount) {
            // Remove materials starting from end.
            const auto iToRemoveCount = vMaterials.size() - iNewMaterialSlotCount;
            for (size_t i = 0; i < iToRemoveCount; i++) {
                // Notify material.
                if (bIsSpawned) {
                    const auto iMaterialSlot = vMaterials.size() - 1;
                    vMaterials[iMaterialSlot]->onSpawnedMeshNodeStoppedUsingMaterial(
                        this, vDeletedIndexBuffers[iMaterialSlot]);
                }

                // Remove material.
                vMaterials.pop_back();
            }

            if (bIsSpawned) {
                // Notify all materials about re-created index buffers
                // (doing it now and not earlier because the number of materials now matches the number
                // of actual material slots).
                for (size_t i = 0; i < vMaterials.size(); i++) {
                    vMaterials[i]->onSpawnedMeshNodeRecreatedIndexBuffer(
                        this, vDeletedIndexBuffers[i], getIndexBufferInfoForMaterialSlot(i));
                }
            }
        } else if (vMaterials.size() < iNewMaterialSlotCount) {
            // Add some slots.
            const auto iOldMaterialCount = vMaterials.size();
            const auto iToAddCount = iNewMaterialSlotCount - iOldMaterialCount;
            for (size_t i = 0; i < iToAddCount; i++) {
                // Prepare material.
                auto pNewMaterial = getDefaultMaterial();

                // First add material.
                vMaterials.push_back(pNewMaterial);

                // Then notify material to properly get information about index buffer for material slot.
                if (bIsSpawned) {
                    pNewMaterial->onSpawnedMeshNodeStartedUsingMaterial(
                        this, getIndexBufferInfoForMaterialSlot(vMaterials.size() - 1));
                }
            }

            if (bIsSpawned) {
                // Notify all old materials about re-created index buffers
                // (doing it now and not earlier because the number of materials now matches the number
                // of actual material slots).
                for (size_t i = 0; i < iOldMaterialCount; i++) {
                    vMaterials[i]->onSpawnedMeshNodeRecreatedIndexBuffer(
                        this, vDeletedIndexBuffers[i], getIndexBufferInfoForMaterialSlot(i));
                }
            }
        } else if (bIsSpawned) {
            // Notify all materials about re-created index buffers.
            for (size_t i = 0; i < vMaterials.size(); i++) {
                vMaterials[i]->onSpawnedMeshNodeRecreatedIndexBuffer(
                    this, vDeletedIndexBuffers[i], getIndexBufferInfoForMaterialSlot(i));
            }
        }

        // Update AABB.
        aabb = AABB::createFromVertices(meshData.getVertices());

        if (!bIsSpawned) {
            // Nothing else to do here.
            return;
        }

        // Notify shader resources.
        updateShaderResourcesToUseChangedMaterialPipelines();

        // Unlock drawing.
        pRenderer->getRenderResourcesMutex()->unlock();
    }

    void MeshNode::onSpawning() {
        // Make sure no rendering happens during the spawn process.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        SpatialNode::onSpawning(); // now call super

        allocateGeometryBuffers();

        // Notify all materials so that the renderer will render this mesh now.
        for (size_t i = 0; i < vMaterials.size(); i++) {
            vMaterials[i]->onMeshNodeSpawning(this, getIndexBufferInfoForMaterialSlot(i));
        }

        // After material was notified (because materials initialize PSOs that shader resources need).
        allocateShaderResources();

        // Bind some shader resources to additional pipelines of our materials.
        updateShaderResourcesToUseChangedMaterialPipelines();
    }

    void MeshNode::onDespawning() {
        // Make sure no rendering happens during the despawn process (since we will delete some resources
        // and also want to avoid deleting resources in the middle of the `draw` function,
        // any resource deletion will cause this thread to wait for renderer to finish its
        // current `draw` function which might create deadlocks if not called right now).
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        SpatialNode::onDespawning(); // now call super

        // Deallocate all resources first (because they reference things from pipeline).
        deallocateShaderResources();

        // Notify all materials so that the renderer will no longer render this mesh
        // and so that materials can free their shader resources and possibly free the pipeline.
        //
        // Materials also reference our index buffers so notify materials first.
        for (size_t i = 0; i < vMaterials.size(); i++) {
            vMaterials[i]->onMeshNodeDespawning(this, getIndexBufferInfoForMaterialSlot(i));
        }

        // Now deallocate vertex/index buffers.
        deallocateGeometryBuffers();
    }

    void* MeshNode::onStartedUpdatingShaderMeshConstants() {
        mtxShaderMeshDataConstants.first.lock();
        return &mtxShaderMeshDataConstants.second;
    }

    void MeshNode::onFinishedUpdatingShaderMeshConstants() { mtxShaderMeshDataConstants.first.unlock(); }

    void MeshNode::setShaderCpuWriteResourceBindingData(
        const std::string& sShaderResourceName,
        size_t iResourceSizeInBytes,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        std::scoped_lock spawnGuard(*getSpawnDespawnMutex(), mtxGpuResources.first);

        // Make sure the node is spawned.
        if (!isSpawned()) [[unlikely]] {
            Error error("binding data to shader resources should be called in `onSpawning` function when the "
                        "node is spawned");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        // keep spawn locked

        // Collect pipelines of all materials.
        std::unordered_set<Pipeline*> pipelinesToUse;
        for (const auto& pMaterial : vMaterials) {
            // Get used pipeline.
            const auto pUsedPipeline = pMaterial->getColorPipeline();
            if (pUsedPipeline == nullptr) [[unlikely]] {
                Error error(std::format(
                    "unable to create shader resources for mesh node \"{}\" because material \"{}\" was not "
                    "initialized",
                    getNodeName(),
                    pMaterial->getMaterialName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Add pipeline.
            pipelinesToUse.insert(pUsedPipeline);
        }

        // Make sure there is no resource with this name.
        auto it = mtxGpuResources.second.shaderResources.shaderCpuWriteResources.find(sShaderResourceName);
        if (it != mtxGpuResources.second.shaderResources.shaderCpuWriteResources.end()) [[unlikely]] {
            Error error(std::format(
                "mesh node \"{}\" already has a shader CPU write resource with the name \"{}\"",
                getNodeName(),
                sShaderResourceName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Create object data constant buffer for shaders.
        const auto pShaderWriteResourceManager =
            getGameInstance()->getWindow()->getRenderer()->getShaderCpuWriteResourceManager();
        auto result = pShaderWriteResourceManager->createShaderCpuWriteResource(
            sShaderResourceName,
            std::format("mesh node \"{}\"", getNodeName()),
            iResourceSizeInBytes,
            pipelinesToUse,
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

    void MeshNode::markShaderCpuWriteResourceToBeCopiedToGpu(const std::string& sShaderResourceName) {
        // In this function we silently exit if some condition is not met and don't log anything
        // intentionally because the user does not check for conditions to be met - it's simpler for the user
        // to write code this way. Moreover, unmet conditions are not errors in this function.

        std::scoped_lock spawnGuard(*getSpawnDespawnMutex(), mtxGpuResources.first);

        // Make sure the node is spawned.
        if (!isSpawned()) {
            return; // silently exit, this is not an error
        }
        // keep spawn locked

        // Make sure there is a resource with this name.
        auto it = mtxGpuResources.second.shaderResources.shaderCpuWriteResources.find(sShaderResourceName);
        if (it == mtxGpuResources.second.shaderResources.shaderCpuWriteResources.end()) {
            return; // silently exit, this is not an error
        }

        // Mark as needs update.
        it->second.markAsNeedsUpdate();
    }

    void MeshNode::setIsVisible(bool bVisible) {
        std::scoped_lock guard(
            *getSpawnDespawnMutex()); // don't change visibility while we are being spawned/despawned

        // Make sure visibility indeed changed.
        if (bIsVisible == bVisible) {
            return;
        }

        // Save new visibility.
        bIsVisible = bVisible;

        if (isSpawned()) {
            // Notify materials.
            for (const auto& pMaterial : vMaterials) {
                pMaterial->onSpawnedMeshNodeChangedVisibility(this, !bIsVisible);
            }
        }
    }

    bool MeshNode::isVisible() const { return bIsVisible; }

    std::vector<std::vector<MeshData::meshindex_t>>* MeshData::getIndices() { return &vIndices; }

    std::vector<MeshVertex>* MeshData::getVertices() { return &vVertices; }

    void MeshNode::updateShaderResourcesToUseChangedMaterialPipelines() {
        std::scoped_lock gpuResourcesGuard(mtxGpuResources.first, *getSpawnDespawnMutex());

        // Make sure we are spawned.
        if (!isSpawned()) [[unlikely]] {
            Error error(std::format("expected the mesh node \"{}\" to be spawned", getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Collect pipelines of all materials.
        std::unordered_set<Pipeline*> pipelinesToUse;
        std::unordered_set<Pipeline*> depthOnlyPipelines;
        std::unordered_set<Pipeline*> shadowMappingPipelines;
        for (const auto& pMaterial : vMaterials) {
            // Get material's used pipeline.
            const auto pPipeline = pMaterial->getColorPipeline();
            if (pPipeline == nullptr) [[unlikely]] {
                Error error(std::format(
                    "expected pipeline of material \"{}\" to be initialized", pMaterial->getMaterialName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Add for shader resources.
            pipelinesToUse.insert(pPipeline);

            // Check if this material also has depth only pipeline.
            const auto pDepthOnlyPipeline = pMaterial->getDepthOnlyPipeline();
            if (pDepthOnlyPipeline != nullptr) {
                // Add it to later bind some special resources to it.
                depthOnlyPipelines.insert(pDepthOnlyPipeline);
            }

            // Check shadow mapping pipeline for directional/spot lights.
            const auto pShadowMappingDirectionalSpotPipeline =
                pMaterial->getShadowMappingDirectionalSpotPipeline();
            if (pShadowMappingDirectionalSpotPipeline != nullptr) {
                // Add it to later bind some special resources to it.
                shadowMappingPipelines.insert(pShadowMappingDirectionalSpotPipeline);
            }

            // Check shadow mapping pipeline for point lights.
            const auto pShadowMappingPointPipeline = pMaterial->getShadowMappingPointPipeline();
            if (pShadowMappingPointPipeline != nullptr) {
                // Add it to later bind some special resources to it.
                shadowMappingPipelines.insert(pShadowMappingPointPipeline);
            }
        }

        // Update shader CPU write resources.
        for (const auto& [sResourceName, shaderCpuWriteResource] :
             mtxGpuResources.second.shaderResources.shaderCpuWriteResources) {
            // Update binding info.
            auto optionalError = shaderCpuWriteResource.getResource()->changeUsedPipelines(pipelinesToUse);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                optionalError->showError();
                throw std::runtime_error(optionalError->getFullErrorMessage());
            }
        }

        if (!depthOnlyPipelines.empty() || !shadowMappingPipelines.empty()) {
            // Additionally, bind mesh data shader resource to depth only / shadow mapping pipelines for depth
            // prepass / shadow mapping since in those passes (only vertex shader) only mesh data is used.

            // Combine usual, depth only and shadow mapping pipelines.
            std::unordered_set<Pipeline*> combinedPipelines = pipelinesToUse;
            for (const auto& pPipeline : depthOnlyPipelines) {
                combinedPipelines.insert(pPipeline);
            }
            for (const auto& pPipeline : shadowMappingPipelines) {
                combinedPipelines.insert(pPipeline);
            }

            // Find mesh data shader resource.
            const auto meshDataIt = mtxGpuResources.second.shaderResources.shaderCpuWriteResources.find(
                sMeshShaderConstantBufferName);
            if (meshDataIt == mtxGpuResources.second.shaderResources.shaderCpuWriteResources.end())
                [[unlikely]] {
                Error error(
                    std::format("expected to find \"{}\" shader resource", sMeshShaderConstantBufferName));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Bind it to all pipelines (including depth only).
            auto optionalError = meshDataIt->second.getResource()->changeUsedPipelines(combinedPipelines);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                optionalError->showError();
                throw std::runtime_error(optionalError->getFullErrorMessage());
            }
        }

#if defined(WIN32) && defined(DEBUG)
        static_assert(
            sizeof(GpuResources::ShaderResources) == 80, "change pipelines of new resources here"); // NOLINT
#elif defined(DEBUG)
        static_assert(
            sizeof(GpuResources::ShaderResources) == 56, "change pipelines of new resources here"); // NOLINT
#endif
    }

    size_t MeshNode::getAvailableMaterialSlotCount() {
        std::scoped_lock guard(*getSpawnDespawnMutex());

        return vMaterials.size();
    }

    std::shared_ptr<Material> MeshNode::getDefaultMaterial() {
        auto result = Material::create(
            EngineShaderNames::MeshNode::getVertexShaderName(),
            EngineShaderNames::MeshNode::getFragmentShaderName(),
            false,
            "Mesh Node's default material");
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return std::get<std::shared_ptr<Material>>(std::move(result));
    }

    void MeshNode::onAfterDeserialized() {
#if defined(DEBUG)
        static_assert(
            sizeof(MeshShaderConstants) == 128, "consider copying deserialized data to shader struct");
#endif

        // Apply any deserialized mesh data.
        onMeshDataChanged();
    }

    std::pair<GpuResource*, unsigned int> MeshNode::getIndexBufferInfoForMaterialSlot(size_t iMaterialSlot) {
        std::scoped_lock guard(*getSpawnDespawnMutex(), mtxGpuResources.first, mtxMeshData);

        // Make sure we are spawned.
        if (!isSpawned()) [[unlikely]] {
            Error error(std::format("expected the mesh node \"{}\" to be spawned", getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure the specified material slot points to an existing index buffer.
        if (iMaterialSlot >= mtxGpuResources.second.mesh.vIndexBuffers.size()) [[unlikely]] {
            Error error(std::format(
                "expected the mesh node \"{}\" index buffer at material slot {} to exist",
                getNodeName(),
                iMaterialSlot));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Save pointer to index buffer.
        const auto pIndexBuffer = mtxGpuResources.second.mesh.vIndexBuffers[iMaterialSlot].get();

        // Now see how much indices in this buffer.
        // Make sure the specified material slots points to an existing index buffer.
        if (iMaterialSlot >= meshData.getIndices()->size()) [[unlikely]] {
            Error error(std::format(
                "expected the mesh node \"{}\" indices at material slot {} to exist",
                getNodeName(),
                iMaterialSlot));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get the number of indices in this slot.
        const auto iIndexCount = static_cast<unsigned int>(meshData.getIndices()->at(iMaterialSlot).size());

        // Self check: make sure the number of elements in the indices array and the number of index buffers
        // is the same.
        if (meshData.getIndices()->size() != mtxGpuResources.second.mesh.vIndexBuffers.size()) [[unlikely]] {
            Error error(std::format(
                "mesh node \"{}\" indices array and index buffers array have different sizes: {} != {}",
                getNodeName(),
                meshData.getIndices()->size(),
                mtxGpuResources.second.mesh.vIndexBuffers.size()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        return {pIndexBuffer, iIndexCount};
    }

}
