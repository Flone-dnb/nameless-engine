#include "World.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"
#include "game/GameManager.h"
#include "game/nodes/Node.h"

namespace ne {

    World::World(GameManager* pGameManager, gc<Node> pRootNode, size_t iWorldSize) : iWorldSize(iWorldSize) {
        // Check that world size is power of 2.
        if (!std::has_single_bit(iWorldSize)) {
            Error err(std::format(
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

        Logger::get().info(std::format("new world with size {} is created", iWorldSize));

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
            Logger::get().error(std::format(
                "destructor for the world object is called but there are still {} node(s) exist "
                "in the world",
                iSpawnedNodeCount));
        }

        // Make sure there are no ticking nodes.
        const auto iCalledEveryFrameNodeCount = getCalledEveryFrameNodeCount();
        if (iCalledEveryFrameNodeCount != 0) [[unlikely]] {
            Logger::get().error(std::format(
                "destructor for the world object is called but there are still {} \"called every frame\" "
                "node(s) exist in the world",
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

    std::pair<std::recursive_mutex, std::unordered_set<Node*>>* World::getReceivingInputNodes() {
        return &mtxReceivingInputNodes;
    }

    void World::onNodeSpawned(Node* pNode) {
        {
            // Get node ID.
            const auto optionalNodeId = pNode->getNodeId();

            // Make sure the node ID is valid.
            if (!optionalNodeId.has_value()) [[unlikely]] {
                Error error(std::format(
                    "node \"{}\" notified the world about being spawned but its ID is invalid",
                    pNode->getNodeName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            const auto iNodeId = optionalNodeId.value();

            std::scoped_lock guard(mtxSpawnedNodes.first);

            // See if we already have a node with this ID.
            const auto it = mtxSpawnedNodes.second.find(iNodeId);
            if (it != mtxSpawnedNodes.second.end()) [[unlikely]] {
                Error error(std::format(
                    "node \"{}\" with ID \"{}\" notified the world about being spawned but there is "
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
                addNodeToCalledEveryFrameArrays(pNode);
            }

            if (pNode->isReceivingInput()) {
                // Add node to array of nodes that receive input.
                std::scoped_lock guard(mtxReceivingInputNodes.first);
                mtxReceivingInputNodes.second.insert(pNode);
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
                Error error(std::format(
                    "node \"{}\" notified the world about being despawned but its ID is invalid",
                    pNode->getNodeName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            const auto iNodeId = optionalNodeId.value();

            std::scoped_lock guard(mtxSpawnedNodes.first);

            // See if we indeed have a node with this ID.
            const auto it = mtxSpawnedNodes.second.find(iNodeId);
            if (it == mtxSpawnedNodes.second.end()) [[unlikely]] {
                Error error(std::format(
                    "node \"{}\" with ID \"{}\" notified the world about being despawned but this node's "
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
            // Force iterate over all ticking nodes and remove this node if it's marked as ticking in our
            // arrays.
            // We don't check `Node::isCalledEveryFrame` here simply because the node can disable it
            // and despawn right after disabling it. If we check `Node::isCalledEveryFrame` we
            // might not remove the node from our arrays and it will continue ticking (error).
            // We even have a test for this bug.
            removeNodeFromCalledEveryFrameArrays(pNode);

            // Not checking for `Node::isReceivingInput` for the same reason as above.
            removeNodeFromReceivingInputArray(pNode);

            iTotalSpawnedNodeCount.fetch_sub(1);
        });
    }

    void World::onSpawnedNodeChangedIsCalledEveryFrame(Node* pNode) {
        // Get node ID.
        const auto optionalNodeId = pNode->getNodeId();

        // Make sure the node ID is valid.
        if (!optionalNodeId.has_value()) [[unlikely]] {
            Error error(std::format("spawned node \"{}\" ID is invalid", pNode->getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Save node ID.
        const auto iNodeId = optionalNodeId.value();

        // Save previous state.
        const auto bPreviousIsCalledEveryFrame = !pNode->isCalledEveryFrame();

        // Modify our arrays as deferred task (see onNodeSpawned for the reason).
        // Additionally, engine guarantees that all deferred tasks will be finished before GC runs
        // so it's safe to do so.
        pGameManager->addDeferredTask([this, pNode, iNodeId, bPreviousIsCalledEveryFrame]() {
            // Make sure this node is still spawned.
            const auto bIsSpawned = isNodeSpawned(iNodeId);
            if (!bIsSpawned) {
                // If it had "is called every frame" enabled it was removed from our arrays during despawn.
                return;
            }

            // Get current setting state.
            const auto bIsCalledEveryFrame = pNode->isCalledEveryFrame();

            if (bIsCalledEveryFrame == bPreviousIsCalledEveryFrame) {
                // The node changed the setting back, nothing to do.
                return;
            }

            if (!bPreviousIsCalledEveryFrame && bIsCalledEveryFrame) {
                // Was disabled but now enabled.
                // Add node to array of nodes that should be called every frame.
                addNodeToCalledEveryFrameArrays(pNode);

                // Log this event since the node is changing this while spawned (might be important to
                // see/debug).
                Logger::get().info(
                    std::format("spawned node \"{}\" is now called every frame", pNode->getNodeName()));
                return;
            }

            if (bPreviousIsCalledEveryFrame && !bIsCalledEveryFrame) {
                // Was enabled but now disabled.
                bool bFound = removeNodeFromCalledEveryFrameArrays(pNode);

                if (bFound) {
                    // Log this event since the node is changing this while spawned (might be important to
                    // see/debug).
                    Logger::get().info(std::format(
                        "spawned node \"{}\" is no longer called every frame", pNode->getNodeName()));
                }
                // else: this might happen if the node had the setting disabled and then quickly
                // enabled and disabled it back
            }
        });
    }

    void World::onSpawnedNodeChangedIsReceivingInput(Node* pNode) {
        // Get node ID.
        const auto optionalNodeId = pNode->getNodeId();

        // Make sure the node ID is valid.
        if (!optionalNodeId.has_value()) [[unlikely]] {
            Error error(std::format("spawned node \"{}\" ID is invalid", pNode->getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Save node ID.
        const auto iNodeId = optionalNodeId.value();

        // Save previous state.
        const auto bPreviousIsReceivingInput = !pNode->isReceivingInput();

        // Modify our array as deferred task (see onNodeSpawned for the reason).
        // Additionally, engine guarantees that all deferred tasks will be finished before GC runs
        // so it's safe to do so.
        pGameManager->addDeferredTask([this, pNode, iNodeId, bPreviousIsReceivingInput]() {
            // Make sure this node is still spawned.
            const auto bIsSpawned = isNodeSpawned(iNodeId);
            if (!bIsSpawned) {
                // If it had "receiving input" enabled it was removed from our array during despawn.
                return;
            }

            // Get current setting state.
            const auto bIsReceivingInput = pNode->isReceivingInput();

            if (bIsReceivingInput == bPreviousIsReceivingInput) {
                // The node changed the setting back, nothing to do.
                return;
            }

            if (!bPreviousIsReceivingInput && bIsReceivingInput) {
                // Was disabled but now enabled.
                // Add node to array of nodes that receive input.
                std::scoped_lock guard(mtxReceivingInputNodes.first);
                mtxReceivingInputNodes.second.insert(pNode);

                // Log this event since the node is changing this while spawned (might be important to
                // see/debug).
                Logger::get().info(
                    std::format("spawned node \"{}\" is now receiving input", pNode->getNodeName()));
                return;
            }

            if (bPreviousIsReceivingInput && !bIsReceivingInput) {
                // Was enabled but now disabled.
                bool bFound = removeNodeFromReceivingInputArray(pNode);

                if (bFound) {
                    // Log this event since the node is changing this while spawned (might be important to
                    // see/debug).
                    Logger::get().info(std::format(
                        "spawned node \"{}\" is no longer receiving input", pNode->getNodeName()));
                }
                // else: this might happen if the node had the setting disabled and then quickly
                // enabled and disabled it back
            }
        });
    }

    void World::addNodeToCalledEveryFrameArrays(Node* pNode) {
        if (pNode->getTickGroup() == TickGroup::FIRST) {
            std::scoped_lock guard(calledEveryFrameNodes.mtxFirstTickGroup.first);
            calledEveryFrameNodes.mtxFirstTickGroup.second.insert(pNode);
        } else {
            std::scoped_lock guard(calledEveryFrameNodes.mtxSecondTickGroup.first);
            calledEveryFrameNodes.mtxSecondTickGroup.second.insert(pNode);
        }
    }

    bool World::removeNodeFromCalledEveryFrameArrays(Node* pNode) {
        // Pick tick group that the node used.
        std::pair<std::recursive_mutex, std::unordered_set<Node*>>* pPairToUse = nullptr;
        if (pNode->getTickGroup() == TickGroup::FIRST) {
            pPairToUse = &calledEveryFrameNodes.mtxFirstTickGroup;
        } else {
            pPairToUse = &calledEveryFrameNodes.mtxSecondTickGroup;
        }

        // Find in array.
        std::scoped_lock guard(pPairToUse->first);
        const auto it = pPairToUse->second.find(pNode);
        if (it == pPairToUse->second.end()) {
            // Not found.
            return false;
        }

        // Remove from array.
        pPairToUse->second.erase(it);

        return true;
    }

    bool World::removeNodeFromReceivingInputArray(Node* pNode) {
        std::scoped_lock guard(mtxReceivingInputNodes.first);

        // Find in array.
        const auto it = mtxReceivingInputNodes.second.find(pNode);
        if (it == mtxReceivingInputNodes.second.end()) {
            // Not found.
            return false;
        }

        // Remove from array.
        mtxReceivingInputNodes.second.erase(it);

        return true;
    }
} // namespace ne
