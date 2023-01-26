#include "World.h"

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"
#include "game/Game.h"

namespace ne {

    World::World(Game* pGame, gc<Node> pRootNode, size_t iWorldSize) {
        // Check that world size is power of 2.
        if (!std::has_single_bit(iWorldSize)) {
            Error err(fmt::format(
                "world size {} should be power of 2 (128, 256, 512, 1024, 2048, etc.).", iWorldSize));
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        // Check if the game instance exists.
        const auto pGameInstance = pGame->getGameInstance();
        if (pGameInstance == nullptr) {
            Error err("an attempt was made to create a new world while GameInstance is not created. "
                      "Are you trying to create a new world in GameInstance's constructor? Use "
                      "`GameInstance::onGameStarted` instead.");
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        // Initialize self.
        this->pGame = pGame;
        this->iWorldSize = iWorldSize;
        mtxIsDestroyed.second = false;
        mtxReceivingInputNodes.second = gc_new_vector<Node>();

        // Spawn root node.
        mtxRootNode.second = std::move(pRootNode);
        mtxRootNode.second->pWorld = this;
        mtxRootNode.second->spawn();

        timeWhenWorldCreated = std::chrono::steady_clock::now();

        Logger::get().info("new world is created", sWorldLogCategory);
    }

    World::~World() { destroyWorld(); }

    std::unique_ptr<World> World::createWorld(Game* pGame, size_t iWorldSize) {
        return std::unique_ptr<World>(new World(pGame, gc_new<Node>("Root"), iWorldSize));
    }

    std::variant<std::unique_ptr<World>, Error>
    World::loadNodeTreeAsWorld(Game* pGame, const std::filesystem::path& pathToNodeTree, size_t iWorldSize) {
        // Deserialize node tree.
        auto result = Node::deserializeNodeTree(pathToNodeTree);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(result);
            error.addEntry();
            return error;
        }

        auto pRootNode = std::get<gc<Node>>(result);

        return std::unique_ptr<World>(new World(pGame, pRootNode, iWorldSize));
    }

    gc<Node> World::getRootNode() {
        std::scoped_lock guard(mtxRootNode.first);
        return mtxRootNode.second;
    }

    size_t World::getCalledEveryFrameNodeCount() {
        std::scoped_lock guard(
            calledEveryFrameNodes.mtxFirstTickGroup.first, calledEveryFrameNodes.mtxSecondTickGroup.first);
        return calledEveryFrameNodes.mtxFirstTickGroup.second->size() +
               calledEveryFrameNodes.mtxSecondTickGroup.second->size();
    }

    float World::getWorldTimeInSeconds() const {
        const auto durationInMs =
            static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - timeWhenWorldCreated)
                                   .count());

        return durationInMs / 1000.0F; // NOLINT
    }

    size_t World::getWorldSize() const { return iWorldSize; }

    void World::destroyWorld() {
        // Mark world as being destroyed.
        std::scoped_lock isDestroyedGuard(mtxIsDestroyed.first);
        if (mtxIsDestroyed.second) {
            return;
        }
        mtxIsDestroyed.second = true;

        Logger::get().info("world is being destroyed, despawning the root node...", sWorldLogCategory);

        // Clear array of nodes that should be called every frame.
        {
            std::scoped_lock guard(
                calledEveryFrameNodes.mtxFirstTickGroup.first,
                calledEveryFrameNodes.mtxSecondTickGroup.first);
            calledEveryFrameNodes.mtxFirstTickGroup.second->clear();
            calledEveryFrameNodes.mtxSecondTickGroup.second->clear();
        }

        // Mark root node as not used explicitly.
        {
            std::scoped_lock guard(mtxRootNode.first);
            mtxRootNode.second->despawn(); // explicitly despawn root to despawn all nodes.
            mtxRootNode.second = nullptr;
        }

        // Run GC.
        pGame->queueGarbageCollection({});
    }

    CalledEveryFrameNodes* World::getCalledEveryFrameNodes() { return &calledEveryFrameNodes; }

    std::pair<std::recursive_mutex, gc_vector<Node>>* World::getReceivingInputNodes() {
        return &mtxReceivingInputNodes;
    }

    void World::onNodeSpawned(const gc<Node>& pNode) {
        // Exit if world is being destroyed.
        std::scoped_lock isDestroyedGuard(mtxIsDestroyed.first);
        if (mtxIsDestroyed.second) {
            return;
        }

        if (pNode->isCalledEveryFrame()) {
            // Add node to array of nodes that should be called every frame.
            if (pNode->getTickGroup() == TickGroup::FIRST) {
                std::scoped_lock guard(calledEveryFrameNodes.mtxFirstTickGroup.first);
                calledEveryFrameNodes.mtxFirstTickGroup.second->push_back(pNode);
            } else {
                std::scoped_lock guard(calledEveryFrameNodes.mtxSecondTickGroup.first);
                calledEveryFrameNodes.mtxSecondTickGroup.second->push_back(pNode);
            }
        }

        if (pNode->receivesInput()) {
            // Add node to array of nodes that receive input.
            std::scoped_lock guard(mtxReceivingInputNodes.first);
            mtxReceivingInputNodes.second->push_back(pNode);
        }
    }

    void World::onNodeDespawned(const gc<Node>& pNode) {
        // Exit if world is being destroyed.
        std::scoped_lock isDestroyedGuard(mtxIsDestroyed.first);
        if (mtxIsDestroyed.second) {
            return;
        }

        if (pNode->isCalledEveryFrame()) {
            // Remove node from array of nodes that should be called every frame.
            bool bFound = false;

            std::pair<std::recursive_mutex, gc_vector<Node>>* pPairToUse = nullptr;
            if (pNode->getTickGroup() == TickGroup::FIRST) {
                pPairToUse = &calledEveryFrameNodes.mtxFirstTickGroup;
            } else {
                pPairToUse = &calledEveryFrameNodes.mtxSecondTickGroup;
            }

            std::scoped_lock guard(pPairToUse->first);
            for (auto it = pPairToUse->second->begin(); it != pPairToUse->second->end(); ++it) {
                if (&*(*it) == &*pNode) {
                    pPairToUse->second->erase(it);
                    bFound = true;
                    break;
                }
            }

            if (!bFound) [[unlikely]] {
                Logger::get().error(
                    fmt::format(
                        "node \"{}\" is marked as \"should be called every frame\" but it does not exist "
                        "in the array of nodes that should be called every frame",
                        pNode->getName()),
                    sWorldLogCategory);
            }
        }

        if (pNode->receivesInput()) {
            // Remove node from array of nodes that receive input.
            bool bFound = false;

            std::scoped_lock guard(mtxReceivingInputNodes.first);
            for (auto it = mtxReceivingInputNodes.second->begin(); it != mtxReceivingInputNodes.second->end();
                 ++it) {
                if (&*(*it) == &*pNode) {
                    mtxReceivingInputNodes.second->erase(it);
                    bFound = true;
                    break;
                }
            }

            if (!bFound) [[unlikely]] {
                Logger::get().error(
                    fmt::format(
                        "node \"{}\" receives input but it does not exist "
                        "in the array of nodes that receive input",
                        pNode->getName()),
                    sWorldLogCategory);
            }
        }
    }

} // namespace ne
