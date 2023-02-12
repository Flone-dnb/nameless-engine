#include "game/nodes/MeshNode.h"

// Custom.
#include "materials/Material.h"
#include "game/Window.h"
#include "render/Renderer.h"
#include "render/general/resources/FrameResourcesManager.h"
#include "render/general/resources/GpuResourceManager.h"
#include "render/general/GpuCommandList.h"
#include "render/general/resources/GpuResource.h"

#include "MeshNode.generated_impl.h"

namespace ne {

    MeshNode::MeshNode() : MeshNode("Mesh Node") {}

    MeshNode::MeshNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
        // Initialize material.
        auto result = Material::create(false);
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

    void MeshNode::onSpawn() {
        SpatialNode::onSpawn();

        // Create constant buffers for shaders.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
        auto result = pRenderer->getResourceManager()->createResourceWithCpuAccess(
            fmt::format("mesh node \"{}\" shader constant buffer", getName()),
            sizeof(MeshShaderConstants),
            FrameResourcesManager::getFrameResourcesCount());
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        pShaderConstantBuffers = std::get<std::unique_ptr<UploadBuffer>>(std::move(result));

        // Bind constant buffer view to the resource.
        auto optionalError = pShaderConstantBuffers->getInternalResource()->bindCbv();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Mark buffers as "needs to be updated".
        iFrameResourceCountToUpdate.store(FrameResourcesManager::getFrameResourcesCount());

        // Notify the material so that the renderer will render this mesh now.
        pMaterial->onMeshNodeSpawned(this);
    }

    void MeshNode::onDespawn() {
        SpatialNode::onDespawn();

        // Notify the material so that the renderer will no longer render this mesh.
        pMaterial->onMeshNodeDespawned(this);

        {
            // Make sure the GPU is not using our constant buffers.
            const auto pRenderer = getGameInstance()->getWindow()->getRenderer();
            std::scoped_lock renderGuard(*pRenderer->getRenderResourcesMutex());
            pRenderer->waitForGpuToFinishWorkUpToThisPoint();

            // Deallocate constant buffers.
            pShaderConstantBuffers = nullptr;
        }
    }

    std::shared_ptr<Material> MeshNode::getMaterial() const { return pMaterial; }

    void MeshNode::onWorldLocationRotationScaleChanged() {
        SpatialNode::onWorldLocationRotationScaleChanged();

        iFrameResourceCountToUpdate.store(FrameResourcesManager::getFrameResourcesCount());
    }

    MeshData::MeshData() {
        // Due to the type we use to store our indices, we expect 32 bits for indices.
        if (sizeof(unsigned int) != 4) [[unlikely]] {
            Error error(fmt::format(
                "expected `unsigned int` type to be 4 bytes long, got: {}", sizeof(unsigned short)));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
    }

    const std::string sPositionsKeyName = "positions";
    const std::string sUvsKeyName = "uvs";

    bool MeshVertex::operator==(const MeshVertex& other) const {
        return position == other.position && uv == other.uv;
    }

    void MeshVertex::serializeVec(
        std::vector<MeshVertex>* pFrom, toml::value* pToml, const std::string& sSectionName) {
        std::vector<float> vPositions;
        std::vector<float> vUvs;
        for (const auto& vertex : *pFrom) {
            vPositions.push_back(vertex.position.x);
            vPositions.push_back(vertex.position.y);
            vPositions.push_back(vertex.position.z);
            vUvs.push_back(vertex.uv.x);
            vUvs.push_back(vertex.uv.y);
        }

        auto table = toml::table();
        table[sPositionsKeyName] = vPositions;
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
            if (!item.is_floating()) [[unlikely]] {
                return Error(fmt::format(
                    "failed to deserialize mesh data: \"{}\" array item is not float", sTomKeyName));
            }
            vData.push_back(static_cast<float>(item.as_floating()));
        }

        // Convert to output.
        std::vector<T> vOutput;
        for (size_t i = 0; i < vData.size(); i += iGlmVecSize) {
            T vec;
            for (size_t j = 0; j < iGlmVecSize; j++) {
                vec.data.data[j] = vData[i + j];
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

        // Get UVs.
        auto uvsResult = deserializeArrayGlmVec<glm::vec2>(pToml, sUvsKeyName);
        if (std::holds_alternative<Error>(uvsResult)) {
            auto error = std::get<Error>(std::move(uvsResult));
            error.addEntry();
            return error;
        }
        const auto vUvs = std::get<std::vector<glm::vec2>>(std::move(uvsResult));

        // Check sizes.
        if (vPositions.size() != vUvs.size()) [[unlikely]] {
            return Error("sizes of deserializes vec arrays are not equal");
        }

        // Save.
        for (size_t i = 0; i < vPositions.size(); i++) {
            MeshVertex vertex;
            vertex.position = vPositions[i];
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

    void MeshNode::onMeshDataChanged() {
        std::scoped_lock guard(mtxMeshData);

        // TODO: recreate GPU resources, etc.
    }

    void MeshNode::draw(GpuCommandList* pCommandList, unsigned int iCurrentFrameResourceIndex) {
        // TODO
    }

    std::pair<std::recursive_mutex*, MeshData*> MeshNode::getMeshData() {
        return std::make_pair(&mtxMeshData, &meshData);
    }

    std::vector<unsigned int>* MeshData::getIndices() { return &vIndices; }

    std::vector<MeshVertex>* MeshData::getVertices() { return &vVertices; }

} // namespace ne
