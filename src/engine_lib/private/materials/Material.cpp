#include "materials/Material.h"

// Custom.
#include "game/Game.h"
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
        this->sName = sMaterialName;
    }

    Material::~Material() {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        // Make sure everything is correct.
        auto iMeshNodeCount = mtxSpawnedMeshNodesThatUseThisMaterial.second.getTotalSize();
        if (iMeshNodeCount != 0) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "material \"{}\" is being destroyed but material's array of spawned mesh nodes "
                    "that use this material still has {} item(s)",
                    sName,
                    iMeshNodeCount),
                sMaterialLogCategory);
        }

        if (pUsedPso.isInitialized()) [[unlikely]] {
            Logger::get().error(
                fmt::format("material \"{}\" is being destroyed but used PSO was not cleared", sName),
                sMaterialLogCategory);
            pUsedPso.clear();
        }

        iTotalMaterialCount.fetch_sub(1);
    }

    size_t Material::getTotalMaterialCount() { return iTotalMaterialCount.load(); }

    void Material::onMeshNodeSpawned(MeshNode* pMeshNode) {
        onSpawnedMeshNodeStartedUsingMaterial(pMeshNode);
    }

    void Material::onSpawnedMeshNodeStartedUsingMaterial(MeshNode* pMeshNode) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        // Make sure we don't have this mesh node in our array.
        if (mtxSpawnedMeshNodesThatUseThisMaterial.second.isMeshNodeAdded(pMeshNode)) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "mesh node \"{}\" notified used material about being spawned but this mesh node already "
                    "exists in material's array of spawned mesh nodes",
                    pMeshNode->getNodeName()),
                sMaterialLogCategory);
            return;
        }

        // Add node to be considered.
        if (pMeshNode->isVisible()) {
            mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.insert(pMeshNode);
        } else {
            mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.insert(pMeshNode);
        }

        // Initialize PSO if needed.
        if (!pUsedPso.isInitialized()) {
            auto result = pPsoManager->getGraphicsPsoForMaterial(
                sVertexShaderName, sPixelShaderName, bUseTransparency, this);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
            pUsedPso = std::get<PsoSharedPtr>(std::move(result));
        }
    }

    void Material::onSpawnedMeshNodeStoppedUsingMaterial(MeshNode* pMeshNode) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        // Make sure we have this mesh node in our array.
        if (!mtxSpawnedMeshNodesThatUseThisMaterial.second.isMeshNodeAdded(pMeshNode)) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "mesh node \"{}\" notified used material about no longer being used but this mesh node "
                    "does not exist in material's array of spawned mesh nodes",
                    pMeshNode->getNodeName()),
                sMaterialLogCategory);
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
            pUsedPso.clear();
        }
    }

    void Material::onMeshNodeDespawned(MeshNode* pMeshNode) {
        onSpawnedMeshNodeStoppedUsingMaterial(pMeshNode);
    }

    std::variant<std::shared_ptr<Material>, Error>
    Material::create(bool bUseTransparency, const std::string& sMaterialName) {
        const auto pGame = Game::get();
        if (pGame == nullptr) {
            return Error("Unable to create material when game object is not created.");
        }

        const auto pRenderer = pGame->getWindow()->getRenderer();
        if (pRenderer == nullptr) {
            return Error("Unable to create material when renderer is not created.");
        }

        return std::shared_ptr<Material>(new Material(
            EngineShaderNames::sVertexShaderName,
            EngineShaderNames::sPixelShaderName,
            bUseTransparency,
            pRenderer->getPsoManager(),
            sMaterialName));
    }

    std::variant<std::shared_ptr<Material>, Error> Material::create(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUseTransparency,
        const std::string& sMaterialName) {
        // Check that the specified custom shaders exist.
        const auto pGame = Game::get();
        if (pGame == nullptr) {
            return Error("Unable to create material when game object is not created.");
        }

        const auto pRenderer = pGame->getWindow()->getRenderer();
        if (pRenderer == nullptr) {
            return Error("Unable to create material when renderer is not created.");
        }

        const auto pShaderManager = pRenderer->getShaderManager();

        // Check vertex shader.
        if (pShaderManager->isShaderNameCanBeUsed(sVertexShaderName)) {
            return Error(
                fmt::format("Vertex shader \"{}\" was not found in the shader manager.", sVertexShaderName));
        }

        // Check pixel shader.
        if (pShaderManager->isShaderNameCanBeUsed(sPixelShaderName)) {
            return Error(
                fmt::format("Pixel shader \"{}\" was not found in the shader manager.", sPixelShaderName));
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

    std::string Material::getName() const { return sName; }

    bool Material::isUsingTransparency() const { return bUseTransparency; }

    void Material::onSpawnedMeshNodeChangedVisibility(MeshNode* pMeshNode, bool bOldVisibility) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        if (bOldVisibility == pMeshNode->isVisible()) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "mesh node \"{}\" notified used material about changed visibility but the visibility "
                    "of this mesh node was not changed",
                    pMeshNode->getNodeName()),
                sMaterialLogCategory);
            return;
        }

        if (bOldVisibility) {
            auto it = mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.find(pMeshNode);
            if (it == mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.end()) [[unlikely]] {
                Logger::get().error(
                    fmt::format(
                        "mesh node \"{}\" notified used material about changed visibility but this mesh node "
                        "does not exist in material's array of spawned mesh nodes",
                        pMeshNode->getNodeName()),
                    sMaterialLogCategory);
                return;
            }
            mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.erase(it);
            mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.insert(pMeshNode);
        } else {
            auto it = mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.find(pMeshNode);
            if (it == mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.end()) [[unlikely]] {
                Logger::get().error(
                    fmt::format(
                        "mesh node \"{}\" notified used material about changed visibility but this mesh node "
                        "does not exist in material's array of spawned mesh nodes",
                        pMeshNode->getNodeName()),
                    sMaterialLogCategory);
                return;
            }
            mtxSpawnedMeshNodesThatUseThisMaterial.second.invisibleMeshNodes.erase(it);
            mtxSpawnedMeshNodesThatUseThisMaterial.second.visibleMeshNodes.insert(pMeshNode);
        }
    }

} // namespace ne
