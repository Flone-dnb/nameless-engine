#include "game/nodes/MeshNode.h"

// Custom.
#include "materials/Material.h"
#include "game/Window.h"
#include "render/Renderer.h"
#include "render/general/resources/GpuResourceManager.h"
#include "materials/ShaderReadWriteResourceManager.h"
#include "materials/EngineShaderNames.hpp"

#include "MeshNode.generated_impl.h"

namespace ne {

    MeshNode::MeshNode() : MeshNode("Mesh Node") {}

    MeshNode::MeshNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
        // Initialize material.
        auto result = Material::create(
            EngineShaderNames::sMeshNodeVertexShaderName,
            EngineShaderNames::sMeshNodePixelShaderName,
            false,
            "Mesh Node's default material");
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        pMaterial = std::get<std::shared_ptr<Material>>(std::move(result));
    }

    void MeshNode::setMaterial(std::shared_ptr<Material> pMaterial) {
        if (pMaterial == nullptr) [[unlikely]] {
            auto error = Error("cannot use `nullptr` as a material");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        std::scoped_lock guard(mtxSpawning);

        if (isSpawned()) {
            this->pMaterial->onSpawnedMeshNodeStoppedUsingMaterial(this);
            pMaterial->onSpawnedMeshNodeStartedUsingMaterial(this);
        }

        this->pMaterial = std::move(pMaterial);
    }

    std::shared_ptr<Material> MeshNode::getMaterial() {
        std::scoped_lock guard(mtxSpawning);
        return pMaterial;
    }

    void MeshNode::onWorldLocationRotationScaleChanged() {
        SpatialNode::onWorldLocationRotationScaleChanged();

        // Update shader constants.
        std::scoped_lock guard(mtxShaderMeshDataConstants.first);
        mtxShaderMeshDataConstants.second.world = getWorldMatrix();

        // Mark as updated.
        markShaderCpuReadWriteResourceAsNeedsUpdate(sMeshShaderConstantBufferName);
    }

    MeshData::MeshData() {
        // Due to the type we use to store our indices, we expect 32 bits for indices.
        // (due to used index format == `DXGI_FORMAT_R32_UINT`)
        constexpr size_t iExpectedSize = 4;
        constexpr auto iActualSize = sizeof(MeshData::meshindex_t);
        if (iActualSize != iExpectedSize) [[unlikely]] {
            Error error(fmt::format(
                "expected mesh index type to be {} bytes long, got: {}", iExpectedSize, iActualSize));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    const std::string sPositionsKeyName = "positions";
    const std::string sUvsKeyName = "uvs";
    const std::string sNormalsKeyName = "normals";

    bool MeshVertex::operator==(const MeshVertex& other) const {
        static_assert(
            sizeof(MeshVertex) == 48, // NOLINT
            "add new fields to `serializeVec`, `deserializeVec`, `operator==` and to unit "
            "tests");
        constexpr auto floatDelta = 0.00001F;
        return glm::all(glm::epsilonEqual(position, other.position, floatDelta)) &&
               glm::all(glm::epsilonEqual(normal, other.normal, floatDelta)) &&
               glm::all(glm::epsilonEqual(uv, other.uv, floatDelta));
    }

    void MeshVertex::serializeVec(
        std::vector<MeshVertex>* pFrom, toml::value* pToml, const std::string& sSectionName) {
        std::vector<std::string> vPositions;
        std::vector<std::string> vUvs;
        std::vector<std::string> vNormals;
        // Store float as string for better precision.
        for (const auto& vertex : *pFrom) {
            vPositions.push_back(toml::format(toml::value(vertex.position.x)));
            vPositions.push_back(toml::format(toml::value(vertex.position.y)));
            vPositions.push_back(toml::format(toml::value(vertex.position.z)));
            vUvs.push_back(toml::format(toml::value(vertex.uv.x)));
            vUvs.push_back(toml::format(toml::value(vertex.uv.y)));
            vNormals.push_back(toml::format(toml::value(vertex.normal.x)));
            vNormals.push_back(toml::format(toml::value(vertex.normal.y)));
            vNormals.push_back(toml::format(toml::value(vertex.normal.z)));
        }

        auto table = toml::table();
        table[sPositionsKeyName] = vPositions;
        table[sNormalsKeyName] = vNormals;
        table[sUvsKeyName] = vUvs;

        pToml->operator[](sSectionName) = table;
    }

    template <typename T>
        requires std::same_as<T, glm::vec2> || std::same_as<T, glm::vec3> || std::same_as<T, glm::vec4>
    std::variant<std::vector<T>, Error>
    deserializeArrayGlmVec(const toml::value* pToml, const std::string& sTomKeyName) {
        if (!pToml->is_table()) {
            return Error("toml value is not a table");
        }

        size_t iGlmVecSize = 0;
        if constexpr (std::is_same_v<T, glm::vec2>) {
            iGlmVecSize = 2;
        } else if (std::is_same_v<T, glm::vec3>) {
            iGlmVecSize = 3;
        } else if (std::is_same_v<T, glm::vec4>) {
            iGlmVecSize = 4;
        } else {
            return Error("unexpected type received");
        }

        // Get data.
        const toml::value& data = pToml->at(sTomKeyName);

        if (!data.is_array()) [[unlikely]] {
            return Error(fmt::format(
                "failed to deserialize mesh data: \"{}\" key does not contain array", sTomKeyName));
        }

        // Convert to array.
        const auto& array = data.as_array();
        if (array.size() % iGlmVecSize != 0) [[unlikely]] {
            return Error(fmt::format(
                "failed to deserialize mesh data: \"{}\" array size is not multiple of {}",
                sTomKeyName,
                iGlmVecSize));
        }

        // Deserialize.
        std::vector<float> vData;
        for (const auto& item : array) {
            // We are storing float as string for better precision.
            if (!item.is_string()) [[unlikely]] {
                return Error(fmt::format(
                    "failed to deserialize mesh data: \"{}\" array item is not string", sTomKeyName));
            }
            try {
                vData.push_back(std::stof(item.as_string().str));
            } catch (std::exception& ex) {
                return Error(fmt::format(
                    "An exception occurred while trying to convert a string to a float: {}", ex.what()));
            }
        }

        // Convert to output.
        std::vector<T> vOutput;
        for (size_t i = 0; i < vData.size(); i += iGlmVecSize) {
            T vec;
            float* pData = glm::value_ptr(vec);
            for (size_t j = 0; j < iGlmVecSize; j++) {
                pData[j] = vData[i + j];
            }
            vOutput.push_back(vec);
        }

        return vOutput;
    }

    std::optional<Error> MeshVertex::deserializeVec(std::vector<MeshVertex>* pTo, const toml::value* pToml) {
        // Get positions.
        auto positionsResult = deserializeArrayGlmVec<glm::vec3>(pToml, sPositionsKeyName);
        if (std::holds_alternative<Error>(positionsResult)) {
            auto error = std::get<Error>(std::move(positionsResult));
            error.addEntry();
            return error;
        }
        const auto vPositions = std::get<std::vector<glm::vec3>>(std::move(positionsResult));

        // Get normals.
        auto normalsResult = deserializeArrayGlmVec<glm::vec3>(pToml, sNormalsKeyName);
        if (std::holds_alternative<Error>(normalsResult)) {
            auto error = std::get<Error>(std::move(normalsResult));
            error.addEntry();
            return error;
        }
        const auto vNormals = std::get<std::vector<glm::vec3>>(std::move(normalsResult));

        // Get UVs.
        auto uvsResult = deserializeArrayGlmVec<glm::vec2>(pToml, sUvsKeyName);
        if (std::holds_alternative<Error>(uvsResult)) {
            auto error = std::get<Error>(std::move(uvsResult));
            error.addEntry();
            return error;
        }
        const auto vUvs = std::get<std::vector<glm::vec2>>(std::move(uvsResult));

        // Check sizes.
        if (vPositions.size() != vUvs.size() || vPositions.size() != vNormals.size()) [[unlikely]] {
            return Error("sizes of deserializes vec arrays are not equal");
        }

        // Save.
        for (size_t i = 0; i < vPositions.size(); i++) {
            MeshVertex vertex;
            vertex.position = vPositions[i];
            vertex.normal = vNormals[i];
            vertex.uv = vUvs[i];

            pTo->push_back(vertex);
        }

        return {};
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
        std::scoped_lock guard(mtxSpawning, mtxGpuResources.first);

        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "mesh node \"{}\" was requested to allocate shader resources but the node is not "
                    "spawned",
                    getNodeName()),
                sMeshNodeLogCategory);
            return;
        }

        prepareDataForBindingToShaderCpuReadWriteResource(
            sMeshShaderConstantBufferName,
            sizeof(MeshShaderConstants),
            [this]() -> void* { return onStartUpdatingShaderMeshConstants(); },
            [this]() { onFinishedUpdatingShaderMeshConstants(); });
    }

    void MeshNode::deallocateShaderResources() {
        std::scoped_lock guard(mtxSpawning, mtxGpuResources.first);

        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "mesh node \"{}\" was requested to deallocate shader resources but the node is "
                    "not spawned",
                    getNodeName()),
                sMeshNodeLogCategory);
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
        std::scoped_lock guard(mtxSpawning, mtxMeshData, mtxGpuResources.first);

        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "mesh node \"{}\" was requested to allocate geometry buffers but the node is not "
                    "spawned",
                    getNodeName()),
                sMeshNodeLogCategory);
            return;
        }

        if (mtxGpuResources.second.mesh.pVertexBuffer != nullptr ||
            mtxGpuResources.second.mesh.pIndexBuffer != nullptr) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "mesh node \"{}\" was requested to deallocate geometry buffers but they are already "
                    "created",
                    getNodeName()),
                sMeshNodeLogCategory);
            return;
        }

        if (meshData.getVertices()->empty()) [[unlikely]] {
            Logger::get().warn(
                fmt::format("mesh node \"{}\" has no mesh vertices", getNodeName()), sMeshNodeLogCategory);
            return; // nothing to create
        }
        if (meshData.getIndices()->empty()) [[unlikely]] {
            Logger::get().warn(
                fmt::format("mesh node \"{}\" has no mesh indices", getNodeName()), sMeshNodeLogCategory);
            return; // nothing to create
        }

        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
        const auto pResourceManager = pRenderer->getResourceManager();

        // Create vertex buffer.
        auto result = pResourceManager->createResourceWithData(
            fmt::format("mesh node \"{}\" vertex buffer", getNodeName()),
            meshData.getVertices()->data(),
            meshData.getVertices()->size() * sizeof(MeshVertex),
            true);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        mtxGpuResources.second.mesh.pVertexBuffer = std::get<std::unique_ptr<GpuResource>>(std::move(result));

        // Create index buffer.
        result = pResourceManager->createResourceWithData(
            fmt::format("mesh node \"{}\" index buffer", getNodeName()),
            meshData.getIndices()->data(),
            meshData.getIndices()->size() * sizeof(MeshData::meshindex_t),
            true);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        mtxGpuResources.second.mesh.pIndexBuffer = std::get<std::unique_ptr<GpuResource>>(std::move(result));
    }

    void MeshNode::deallocateGeometryBuffers() {
        std::scoped_lock guard(mtxSpawning, mtxMeshData, mtxGpuResources.first);

        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "mesh node \"{}\" was requested to deallocate geometry buffers but the node is not "
                    "spawned",
                    getNodeName()),
                sMeshNodeLogCategory);
            return;
        }

        if (mtxGpuResources.second.mesh.pVertexBuffer == nullptr ||
            mtxGpuResources.second.mesh.pIndexBuffer == nullptr) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "mesh node \"{}\" was requested to deallocate geometry buffers but they were not "
                    "created previously",
                    getNodeName()),
                sMeshNodeLogCategory);
            return;
        }

        // Make sure the GPU is not using our buffers.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
        std::scoped_lock renderGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Deallocate buffers.
        mtxGpuResources.second.mesh.pVertexBuffer = nullptr;
        mtxGpuResources.second.mesh.pIndexBuffer = nullptr;
    }

    void MeshNode::onMeshDataChanged() {
        std::scoped_lock guard(mtxMeshData, mtxSpawning);
        if (!isSpawned()) {
            return;
        }

        // Make sure we don't exceed type limit for vertices.
        const auto iVertexCount = meshData.getVertices()->size();
        if (iVertexCount > UINT_MAX) [[unlikely]] {
            // This will exceed type limit for DirectX vertex buffer view fields.
            auto error = Error(fmt::format(
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
            auto error = Error(fmt::format(
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
        const auto iIndexCount = meshData.getIndices()->size();
        const auto iIndexBufferSize = iIndexCount * sizeof(MeshData::meshindex_t);
        if (iIndexBufferSize > UINT_MAX) [[unlikely]] {
            // This will exceed type limit for DirectX vertex buffer view fields.
            auto error = Error(fmt::format(
                "size of the index buffer ({} bytes) for the mesh node \"{}\" will exceed the limit of {} "
                "bytes ({} indices * {} index size = {}), can't continue because an overflow will occur",
                iIndexBufferSize,
                getNodeName(),
                UINT_MAX,
                iIndexCount,
                sizeof(MeshData::meshindex_t),
                iIndexBufferSize));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        deallocateGeometryBuffers();
        allocateGeometryBuffers();
    }

    void MeshNode::onSpawn() {
        // Make sure no rendering happens during the spawn process.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        SpatialNode::onSpawn();

        allocateGeometryBuffers();

        // Notify the material so that the renderer will render this mesh now.
        pMaterial->onMeshNodeSpawned(this);

        // After material was notified (because materials initialize PSOs that shader resources need).
        allocateShaderResources();
    }

    void MeshNode::onDespawn() {
        // Make sure no rendering happens during the despawn process.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
        std::scoped_lock drawGuard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        SpatialNode::onDespawn();

        // Notify the material so that the renderer will no longer render this mesh.
        pMaterial->onMeshNodeDespawned(this);

        deallocateShaderResources();
        deallocateGeometryBuffers();
    }

    void* MeshNode::onStartUpdatingShaderMeshConstants() {
        mtxShaderMeshDataConstants.first.lock();
        return &mtxShaderMeshDataConstants.second;
    }

    void MeshNode::onFinishedUpdatingShaderMeshConstants() { mtxShaderMeshDataConstants.first.unlock(); }

    void MeshNode::prepareDataForBindingToShaderCpuReadWriteResource(
        const std::string& sShaderResourceName,
        size_t iResourceSizeInBytes,
        const std::function<void*()>& onStartedUpdatingResource,
        const std::function<void()>& onFinishedUpdatingResource) {
        // Make sure the node is spawned.
        std::scoped_lock spawnGuard(mtxSpawning);
        if (!isSpawned()) [[unlikely]] {
            Error error("binding data to shader resources should be called in `onSpawn` function when the "
                        "node is spawned");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        // keep spawn locked

        // Make sure material was initialized.
        const auto pUsedPso = pMaterial->getUsedPso();
        if (pUsedPso == nullptr) [[unlikely]] {
            Error error(fmt::format(
                "unable to create shader resources for mesh node \"{}\" because material was not initialized",
                getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        std::scoped_lock gpuResourceGuard(mtxGpuResources.first);

        // Make sure there is no resource with this name.
        auto it =
            mtxGpuResources.second.shaderResources.shaderCpuReadWriteResources.find(sShaderResourceName);
        if (it != mtxGpuResources.second.shaderResources.shaderCpuReadWriteResources.end()) [[unlikely]] {
            Error error(fmt::format(
                "mesh node \"{}\" already has a shader CPU read/write resource with the name \"{}\"",
                getNodeName(),
                sShaderResourceName));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Create object data constant buffer for shaders.
        const auto pShaderReadWriteResourceManager =
            getGameInstance()->getWindow()->getRenderer()->getShaderCpuReadWriteResourceManager();
        auto result = pShaderReadWriteResourceManager->createShaderCpuReadWriteResource(
            sShaderResourceName,
            fmt::format("mesh node \"{}\"", getNodeName()),
            iResourceSizeInBytes,
            pUsedPso,
            onStartedUpdatingResource,
            onFinishedUpdatingResource);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Add to be considered.
        mtxGpuResources.second.shaderResources.shaderCpuReadWriteResources[sShaderResourceName] =
            std::get<ShaderCpuReadWriteResourceUniquePtr>(std::move(result));
    }

    void MeshNode::markShaderCpuReadWriteResourceAsNeedsUpdate(const std::string& sShaderResourceName) {
        // Make sure the node is spawned.
        std::scoped_lock spawnGuard(mtxSpawning);
        if (!isSpawned()) {
            return; // silently exit
        }
        // keep spawn locked

        std::scoped_lock gpuResourceGuard(mtxGpuResources.first);

        // Make sure there is a resource with this name.
        auto it =
            mtxGpuResources.second.shaderResources.shaderCpuReadWriteResources.find(sShaderResourceName);
        if (it == mtxGpuResources.second.shaderResources.shaderCpuReadWriteResources.end()) {
            return; // silently exit
        }

        // Mark as needs update.
        it->second.markAsNeedsUpdate();
    }

    void MeshNode::setVisibility(bool bVisible) {
        std::scoped_lock guard(mtxSpawning); // don't change visibility while we are being spawned

        if (bIsVisible == bVisible) {
            return;
        }

        bIsVisible = bVisible;

        pMaterial->onSpawnedMeshNodeChangedVisibility(this, !bIsVisible);
    }

    bool MeshNode::isVisible() const { return bIsVisible; }

    std::vector<MeshData::meshindex_t>* MeshData::getIndices() { return &vIndices; }

    std::vector<MeshVertex>* MeshData::getVertices() { return &vVertices; }

} // namespace ne
