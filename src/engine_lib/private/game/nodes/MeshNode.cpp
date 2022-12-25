#include "game/nodes/MeshNode.h"

// Custom.
#include "materials/Material.h"

namespace ne {

    MeshNode::MeshNode() : MeshNode("Mesh Node") {}

    MeshNode::MeshNode(const std::string& sNodeName) : SpatialNode(sNodeName) {
        // Initialize material.
        auto result = Material::create(false);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.showError();
            throw std::runtime_error(error.getError());
        }
        pMaterial = std::get<gc<Material>>(std::move(result));
    }

    void MeshNode::setMaterial(gc<Material> pMaterial) {
        if (pMaterial == nullptr) [[unlikely]] {
            auto error = Error("cannot use `nullptr` as a material");
            error.showError();
            throw std::runtime_error(error.getError());
        }

        std::scoped_lock guard(mtxSpawning);

        if (isSpawned()) {
            this->pMaterial->onSpawnedMeshNodeStoppedUsingMaterial(this);
            pMaterial->onSpawnedMeshNodeStartedUsingMaterial(this);
        }

        this->pMaterial = pMaterial;
    }

    void MeshNode::onSpawn() {
        SpatialNode::onSpawn();
        pMaterial->onMeshNodeSpawned(this);
    }

    void MeshNode::onDespawn() {
        SpatialNode::onDespawn();
        pMaterial->onMeshNodeDespawned(this);
    }

} // namespace ne
