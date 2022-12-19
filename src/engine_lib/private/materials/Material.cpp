#include "materials/Material.h"

// Custom.
#include "game/Game.h"
#include "game/Window.h"
#include "io/Logger.h"
#include "game/nodes/MeshNode.h"
#include "materials/EngineShaderNames.hpp"

namespace ne {

    Material::Material(
        const std::string& sVertexShaderName,
        const std::string& sPixelShaderName,
        bool bUseTransparency,
        PsoManager* pPsoManager,
        const std::string& sMaterialName) {
        this->sVertexShaderName = sVertexShaderName;
        this->sPixelShaderName = sPixelShaderName;
        this->bUseTransparency = bUseTransparency;
        this->pPsoManager = pPsoManager;
        this->sName = sMaterialName;
    }

    Material::~Material() {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        // Make sure everything is correct.
        const auto iMeshNodeCount = mtxSpawnedMeshNodesThatUseThisMaterial.second.size();
        if (iMeshNodeCount != 0) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "material \"{}\" is being destroyed but material's array of spawned mesh nodes that use "
                    "this material still has {} item(s)",
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
    }

    void Material::onMeshNodeSpawned(MeshNode* pMeshNode) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        // Make sure we don't have this mesh node in our array.
        const auto it = mtxSpawnedMeshNodesThatUseThisMaterial.second.find(pMeshNode);
        if (it != mtxSpawnedMeshNodesThatUseThisMaterial.second.end()) {
            Logger::get().error(
                fmt::format(
                    "mesh node \"{}\" notified used material about being spawned but this mesh node already "
                    "exists in material's array of spawned mesh nodes",
                    pMeshNode->getName()),
                sMaterialLogCategory);
            return;
        }

        mtxSpawnedMeshNodesThatUseThisMaterial.second.insert(pMeshNode);

        // Initialize PSO if needed.
        if (!pUsedPso.isInitialized()) {
            auto result = pPsoManager->getGraphicsPsoForMaterial(
                sVertexShaderName, sPixelShaderName, bUseTransparency, this);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                error.showError();
                throw std::runtime_error(error.getError());
            }
            pUsedPso = std::get<PsoSharedPtr>(std::move(result));
        }
    }

    void Material::onSpawnedMeshNodeStartedUsingMaterial(MeshNode* pMeshNode) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        // Make sure we don't have this mesh node in our array.
        const auto it = mtxSpawnedMeshNodesThatUseThisMaterial.second.find(pMeshNode);
        if (it != mtxSpawnedMeshNodesThatUseThisMaterial.second.end()) {
            Logger::get().error(
                fmt::format(
                    "mesh node \"{}\" notified a material about being used but this mesh node already "
                    "exists in material's array of spawned mesh nodes",
                    pMeshNode->getName()),
                sMaterialLogCategory);
            return;
        }

        mtxSpawnedMeshNodesThatUseThisMaterial.second.insert(pMeshNode);

        // Initialize PSO if needed.
        if (!pUsedPso.isInitialized()) {
            auto result = pPsoManager->getGraphicsPsoForMaterial(
                sVertexShaderName, sPixelShaderName, bUseTransparency, this);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(std::move(result));
                error.addEntry();
                error.showError();
                throw std::runtime_error(error.getError());
            }
            pUsedPso = std::get<PsoSharedPtr>(std::move(result));
        }
    }

    void Material::onSpawnedMeshNodeStoppedUsingMaterial(MeshNode* pMeshNode) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        // Make sure we have this mesh node in our array.
        const auto it = mtxSpawnedMeshNodesThatUseThisMaterial.second.find(pMeshNode);
        if (it == mtxSpawnedMeshNodesThatUseThisMaterial.second.end()) {
            Logger::get().error(
                fmt::format(
                    "mesh node \"{}\" notified used material about no longer being used but this mesh node "
                    "does not exist in material's array of spawned mesh nodes",
                    pMeshNode->getName()),
                sMaterialLogCategory);
            return;
        }

        mtxSpawnedMeshNodesThatUseThisMaterial.second.erase(it);

        // Check if need to free PSO.
        if (mtxSpawnedMeshNodesThatUseThisMaterial.second.empty()) {
            pUsedPso.clear();
        }
    }

    void Material::onMeshNodeDespawned(MeshNode* pMeshNode) {
        std::scoped_lock guard(mtxSpawnedMeshNodesThatUseThisMaterial.first);

        // Make sure we have this mesh node in our array.
        const auto it = mtxSpawnedMeshNodesThatUseThisMaterial.second.find(pMeshNode);
        if (it == mtxSpawnedMeshNodesThatUseThisMaterial.second.end()) {
            Logger::get().error(
                fmt::format(
                    "mesh node \"{}\" notified used material about being despawned but this mesh node "
                    "does not exist in material's array of spawned mesh nodes",
                    pMeshNode->getName()),
                sMaterialLogCategory);
            return;
        }

        mtxSpawnedMeshNodesThatUseThisMaterial.second.erase(it);

        // Check if need to free PSO.
        if (mtxSpawnedMeshNodesThatUseThisMaterial.second.empty()) {
            pUsedPso.clear();
        }
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
        const std::string& sCustomVertexShaderName,
        const std::string& sCustomPixelShaderName,
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
        if (pShaderManager->isShaderNameCanBeUsed(sCustomVertexShaderName)) {
            return Error(fmt::format(
                "Vertex shader \"{}\" was not found in the shader manager.", sCustomVertexShaderName));
        }

        // Check pixel shader.
        if (pShaderManager->isShaderNameCanBeUsed(sCustomPixelShaderName)) {
            return Error(fmt::format(
                "Pixel shader \"{}\" was not found in the shader manager.", sCustomPixelShaderName));
        }

        // Create material.
        return std::shared_ptr<Material>(new Material(
            sCustomVertexShaderName,
            sCustomPixelShaderName,
            bUseTransparency,
            pRenderer->getPsoManager(),
            sMaterialName));
    }

    std::pair<std::mutex, std::set<MeshNode*>>* Material::getSpawnedMeshNodesThatUseThisMaterial() {
        return &mtxSpawnedMeshNodesThatUseThisMaterial;
    }

    std::string Material::getName() const { return sName; }

} // namespace ne
