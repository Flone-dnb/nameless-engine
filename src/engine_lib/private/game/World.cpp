#include "World.h"

// Custom.
#include "io/Logger.h"
#include "game/GameManager.h"
#include "game/nodes/Node.h"

namespace ne {

    World::World(GameManager* pGameManager, gc<Node> pRootNode, size_t iWorldSize) : iWorldSize(iWorldSize) {
        // Check that world size is power of 2.
        if (!std::has_single_bit(iWorldSize)) {
            Error err(fmt::format(
                "world size {} should be power of 2 (128, 256, 512, 1024, 2048, etc.).", iWorldSize));
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        // Check if the game instance exists.
        const auto pGameInstance = pGameManager->getGameInstance();
        if (pGameInstance == nullptr) {
            Error err("an attempt was made to create a new world while GameInstance is not created. "
                      "Are you trying to create a new world in GameInstance's constructor? Use "
                      "`GameInstance::onGameStarted` instead.");
            err.showError();
            throw std::runtime_error(err.getFullErrorMessage());
        }

        Logger::get().info(fmt::format("new world with size {} is created", iWorldSize));

        // Initialize self.
        this->pGameManager = pGameManager;
        mtxIsDestroyed.second = false;

        // Spawn root node.
        mtxRootNode.second = std::move(pRootNode);
        mtxRootNode.second->pWorld = this;
        mtxRootNode.second->spawn();

        timeWhenWorldCreated = std::chrono::steady_clock::now();
    }

    World::~World() {
        // Make sure world was marked as destroyed.
        std::scoped_lock isDestroyedGuard(mtxIsDestroyed.first);
        if (!mtxIsDestroyed.second) [[unlikely]] {
            Logger::get().error("destructor for the world object is called but the world was not destroyed");
        }

        // Make sure all nodes were despawned.
        const auto iSpawnedNodeCount = iTotalSpawnedNodeCount.load();
        if (iSpawnedNodeCount != 0) [[unlikely]] {
            Logger::get().error(fmt::format(
                "destructor for the world object is called but there are still {} node(s) exist "
                "in the world",
                iSpawnedNodeCount));
        }
    }

    std::unique_ptr<World> World::createWorld(GameManager* pGameManager, size_t iWorldSize) {
        return std::unique_ptr<World>(new World(pGameManager, gc_new<Node>("Root"), iWorldSize));
    }

    std::variant<std::unique_ptr<World>, Error> World::loadNodeTreeAsWorld(
        GameManager* pGameManager, const std::filesystem::path& pathToNodeTree, size_t iWorldSize) {
        // Deserialize node tree.
        auto result = Node::deserializeNodeTree(pathToNodeTree);
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(result);
            error.addCurrentLocationToErrorStack();
            return error;
        }

        auto pRootNode = std::get<gc<Node>>(result);

        return std::unique_ptr<World>(new World(pGameManager, pRootNode, iWorldSize));
    }

    size_t World::getTotalSpawnedNodeCount() { return iTotalSpawnedNodeCount.load(); }

    gc<Node> World::getRootNode() {
        std::scoped_lock guard(mtxRootNode.first);
        return mtxRootNode.second;
    }

    size_t World::getCalledEveryFrameNodeCount() {
        std::scoped_lock guard(
            calledEveryFrameNodes.mtxFirstTickGroup.first, calledEveryFrameNodes.mtxSecondTickGroup.first);
        return calledEveryFrameNodes.mtxFirstTickGroup.second.size() +
               calledEveryFrameNodes.mtxSecondTickGroup.second.size();
    }

    float World::getWorldTimeInSeconds() const {
        const auto durationInSec = std::chrono::duration<float, std::chrono::seconds::period>(
                                       std::chrono::steady_clock::now() - timeWhenWorldCreated)
                                       .count();
        return durationInSec;
    }

    size_t World::getWorldSize() const { return iWorldSize; }

    bool World::isNodeSpawned(size_t iNodeId) {
        std::scoped_lock guard(mtxSpawnedNodes.first);

        const auto it = mtxSpawnedNodes.second.find(iNodeId);

        return it != mtxSpawnedNodes.second.end();
    }

    void World::destroyWorld() {
        // Mark world as being destroyed.
        std::scoped_lock isDestroyedGuard(mtxIsDestroyed.first);
        if (mtxIsDestroyed.second) {
            return;
        }
        mtxIsDestroyed.second = true;

        Logger::get().info("world is being destroyed, despawning world's root node...");

        // Mark root node as not used explicitly.
        {
            std::scoped_lock guard(mtxRootNode.first);
            mtxRootNode.second->despawn(); // explicitly despawn root to despawn all nodes.
            mtxRootNode.second = nullptr;
        }

        // Don't clear any arrays, they will be cleared in deferred tasks once nodes are despawned.
    }

    CalledEveryFrameNodes* World::getCalledEveryFrameNodes() { return &calledEveryFrameNodes; }

    std::pair<std::recursive_mutex, std::vector<Node*>>* World::getReceivingInputNodes() {
        return &mtxReceivingInputNodes;
    }

    void World::onNodeSpawned(Node* pNode) {
        {
            // Get node ID.
            const auto optionalNodeId = pNode->getNodeId();

            // Make sure the node ID is valid.
            if (!optionalNodeId.has_value()) [[unlikely]] {
                Error error(fmt::format(
                    "the node \"{}\" notified the world about being spawned but its ID is invalid",
                    pNode->getNodeName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            const auto iNodeId = optionalNodeId.value();

            std::scoped_lock guard(mtxSpawnedNodes.first);

            // See if we already have a node with this ID.
            const auto it = mtxSpawnedNodes.second.find(iNodeId);
            if (it != mtxSpawnedNodes.second.end()) [[unlikely]] {
                Error error(fmt::format(
                    "the node \"{}\" with ID \"{}\" notified the world about being spawned but there is "
                    "already a spawned node with this ID",
                    pNode->getNodeName(),
                    iNodeId));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Save node.
            mtxSpawnedNodes.second[iNodeId] = pNode;
        }

        // Add to our arrays as deferred task. Why I've decided to do it as a deferred task:
        // if we are right now iterating over an array of nodes that world stores (for ex. ticking, where
        // one node inside of its tick function decided to spawn another node would cause us to get here)
        // without deferred task we will modify array that we are iterating over which will cause
        // bad things.
        pGameManager->addDeferredTask([this, pNode]() {
            if (pNode->isCalledEveryFrame()) {
                // Add node to array of nodes that should be called every frame.
                if (pNode->getTickGroup() == TickGroup::FIRST) {
                    std::scoped_lock guard(calledEveryFrameNodes.mtxFirstTickGroup.first);
                    calledEveryFrameNodes.mtxFirstTickGroup.second.push_back(pNode);
                } else {
                    std::scoped_lock guard(calledEveryFrameNodes.mtxSecondTickGroup.first);
                    calledEveryFrameNodes.mtxSecondTickGroup.second.push_back(pNode);
                }
            }

            if (pNode->receivesInput()) {
                // Add node to array of nodes that receive input.
                std::scoped_lock guard(mtxReceivingInputNodes.first);
                mtxReceivingInputNodes.second.push_back(pNode);
            }

            iTotalSpawnedNodeCount.fetch_add(1);
        });
    }

    void World::onNodeDespawned(Node* pNode) {
        {
            // Get node ID.
            const auto optionalNodeId = pNode->getNodeId();

            // Make sure the node ID is valid.
            if (!optionalNodeId.has_value()) [[unlikely]] {
                Error error(fmt::format(
                    "the node \"{}\" notified the world about being despawned but its ID is invalid",
                    pNode->getNodeName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            const auto iNodeId = optionalNodeId.value();

            std::scoped_lock guard(mtxSpawnedNodes.first);

            // See if we indeed have a node with this ID.
            const auto it = mtxSpawnedNodes.second.find(iNodeId);
            if (it == mtxSpawnedNodes.second.end()) [[unlikely]] {
                Error error(fmt::format(
                    "the node \"{}\" with ID \"{}\" notified the world about being despawned but this node's "
                    "ID is not found",
                    pNode->getNodeName(),
                    iNodeId));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Remove node.
            mtxSpawnedNodes.second.erase(it);
        }

        // Remove to our arrays as deferred task (see onNodeSpawned for the reason).
        // Additionally, engine guarantees that all deferred tasks will be finished before GC runs
        // so it's safe to do so.
        pGameManager->addDeferredTask([this, pNode]() {
            if (pNode->isCalledEveryFrame()) {
                // Remove node from array of nodes that should be called every frame.
                bool bFound = false;

                std::pair<std::recursive_mutex, std::vector<Node*>>* pPairToUse = nullptr;
                if (pNode->getTickGroup() == TickGroup::FIRST) {
                    pPairToUse = &calledEveryFrameNodes.mtxFirstTickGroup;
                } else {
                    pPairToUse = &calledEveryFrameNodes.mtxSecondTickGroup;
                }

                std::scoped_lock guard(pPairToUse->first);
                for (auto it = pPairToUse->second.begin(); it != pPairToUse->second.end(); ++it) {
                    if ((*it) == pNode) {
                        pPairToUse->second.erase(it);
                        bFound = true;
                        break;
                    }
                }

                if (!bFound) [[unlikely]] {
                    Logger::get().error(fmt::format(
                        "node \"{}\" is marked as \"should be called every frame\" but it does not exist "
                        "in the array of nodes that should be called every frame",
                        pNode->getNodeName()));
                }
            }

            if (pNode->receivesInput()) {
                // Remove node from array of nodes that receive input.
                bool bFound = false;

                std::scoped_lock guard(mtxReceivingInputNodes.first);
                for (auto it = mtxReceivingInputNodes.second.begin();
                     it != mtxReceivingInputNodes.second.end();
                     ++it) {
                    if ((*it) == pNode) {
                        mtxReceivingInputNodes.second.erase(it);
                        bFound = true;
                        break;
                    }
                }

                if (!bFound) [[unlikely]] {
                    Logger::get().error(fmt::format(
                        "node \"{}\" receives input but it does not exist "
                        "in the array of nodes that receive input",
                        pNode->getNodeName()));
                }
            }

            iTotalSpawnedNodeCount.fetch_sub(1);
        });
    }

} // namespace ne
