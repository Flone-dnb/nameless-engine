#include "game/nodes/Node.h"

// STL.
#include <ranges>

// Custom.
#include "io/Logger.h"
#include "misc/Globals.h"
#include "game/GameInstance.h"

#if defined(DEBUG)
/** Total amount of alive nodes. */
static std::atomic<size_t> iTotalAliveNodeCount{0};
#endif

namespace ne {
    size_t Node::getAliveNodeCount() { return iTotalAliveNodeCount.load(); }

    Node::Node() : Node("Node") {}

    Node::Node(const std::string& sName) {
        this->sName = sName;
        mtxParentNode.second = nullptr;
        bIsSpawned = false;
        mtxChildNodes.second = gc_new_vector<Node>();

        // Log construction.
        const size_t iNodeCount = iTotalAliveNodeCount.fetch_add(1) + 1;
        Logger::get().info(
            fmt::format("constructor for node \"{}\" is called (alive nodes now: {})", sName, iNodeCount),
            sNodeLogCategory);
    }

    Node::~Node() {
        std::scoped_lock guard(mtxSpawning);
        if (bIsSpawned) {
            despawn();
        }

        // Log destruction.
        const size_t iNodesLeft = iTotalAliveNodeCount.fetch_sub(1) - 1;
        Logger::get().info(
            fmt::format("destructor for node \"{}\" is called (alive nodes left: {})", sName, iNodesLeft),
            sNodeLogCategory);
    }

    void Node::setName(const std::string& sName) { this->sName = sName; }

    std::string Node::getName() const { return sName; }

    void Node::addChildNode(gc<Node> pNode) {
        std::scoped_lock spawnGuard(mtxSpawning);

        // Check if this node is valid.
        if (pNode == nullptr) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "an attempt was made to attach a nullptr node to the \"{}\" node, aborting this "
                    "operation",
                    getName()),
                sNodeLogCategory);
            return;
        }

        // Check if this node is `this`.
        if (&*pNode == this) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "an attempt was made to attach the \"{}\" node to itself, aborting this "
                    "operation",
                    getName()),
                sNodeLogCategory);
            return;
        }

        std::scoped_lock guard(mtxChildNodes.first);

        // Check if this node is already our child.
        for (const auto& pChildNode : *mtxChildNodes.second) {
            if (pChildNode == pNode) [[unlikely]] {
                Logger::get().warn(
                    fmt::format(
                        "an attempt was made to attach the \"{}\" node to the \"{}\" node but it's already "
                        "a child node of \"{}\", aborting this operation",
                        pNode->getName(),
                        getName(),
                        getName()),
                    sNodeLogCategory);
                return;
            }
        }

        // Check if this node is already attached to some node.
        std::scoped_lock parentGuard(pNode->mtxParentNode.first);
        if (pNode->mtxParentNode.second != nullptr) {
            // Check if we are already this node's parent.
            if (&*pNode->mtxParentNode.second == this) {
                Logger::get().warn(
                    fmt::format(
                        "an attempt was made to attach the \"{}\" node to its parent again, "
                        "aborting this operation",
                        pNode->getName()),
                    sNodeLogCategory);
                return;
            }

            // Notify start of detachment.
            pNode->onBeforeDetachedFromParent();

            // Remove node from parent's children array.
            std::scoped_lock parentsChildrenGuard(pNode->mtxParentNode.second->mtxChildNodes.first);

            const auto pParentsChildren = &pNode->mtxParentNode.second->mtxChildNodes.second;
            for (auto it = (*pParentsChildren)->begin(); it != (*pParentsChildren)->end(); ++it) {
                if (*it == pNode) {
                    (*pParentsChildren)->erase(it);
                    break;
                }
            }
        }

        // Add node to our children array.
        pNode->mtxParentNode.second = gc<Node>(this);
        mtxChildNodes.second->push_back(pNode);

        // Notify.
        pNode->onAfterAttachedToNewParent();

        // don't unlock node's parent lock here yet, still doing some logic based on the new parent

        // Spawn/despawn node if needed.
        if (isSpawned() && !pNode->isSpawned()) {
            // Spawn node.
            pNode->spawn();
        } else if (!isSpawned() && pNode->isSpawned()) {
            // Despawn node.
            pNode->despawn();
        }
    }

    void Node::detachFromParentAndDespawn() {
        // Check if this node is spawned.
        std::scoped_lock guard(mtxSpawning);
        if (bIsSpawned == false)
            return;

        // Detach from parent.
        std::scoped_lock parentGuard(mtxParentNode.first);
        gc<Node> pSelf;

        if (mtxParentNode.second != nullptr) {
            // Notify.
            onBeforeDetachedFromParent();

            // Remove this node from parent's children array.
            std::scoped_lock parentChildGuard(mtxParentNode.second->mtxChildNodes.first);
            auto pParentChildren = &mtxParentNode.second->mtxChildNodes.second;
            bool bFound = false;
            for (auto it = (*pParentChildren)->begin(); it != (*pParentChildren)->end(); ++it) {
                if (&**it == this) {
                    pSelf = *it; // make sure we are not going to be deleted while in this function
                    (*pParentChildren)->erase(it);
                    bFound = true;
                    break;
                }
            }

            if (!bFound) [[unlikely]] {
                Logger::get().error(
                    fmt::format(
                        "Node \"{}\" has a parent node but parent's children array "
                        "does not contain this node.",
                        getName()),
                    sNodeLogCategory);
            }

            // Remove parent.
            mtxParentNode.second = nullptr;
        }

        // Despawn.
        despawn();

        // Queue garbage collection.
        pSelf = nullptr;
        pGameInstance->queueGarbageCollection({});
    }

    bool Node::isSpawned() {
        std::scoped_lock guard(mtxSpawning);
        return bIsSpawned;
    }

    GameInstance* Node::findValidGameInstance() {
        std::scoped_lock guard(mtxSpawning);

        if (pGameInstance)
            return pGameInstance;

        // Ask parents for the valid game instance pointer.
        std::scoped_lock parentGuard(mtxParentNode.first);
        if (!mtxParentNode.second) [[unlikely]] {
            Error err(fmt::format(
                "node \"{}\" can't get a pointer to the valid game instance because "
                "there is no parent node",
                getName()));
            err.showError();
            throw std::runtime_error(err.getError());
        }

        return mtxParentNode.second->findValidGameInstance();
    }

    void Node::spawn() {
        std::scoped_lock guard(mtxSpawning);

        if (bIsSpawned) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "an attempt was made to spawn already spawned node \"{}\", aborting this operation",
                    getName()),
                sNodeLogCategory);
            return;
        }

        if (!pGameInstance) {
            pGameInstance = findValidGameInstance();
        }

        // Spawn self first and only then child nodes.
        // This spawn order is required for some nodes to work correctly.
        // With this spawn order we will not make "holes" in world's node tree
        // (i.e. when node is spawned, node's parent is not spawned but parent's parent node is spawned).
        bIsSpawned = true;
        onSpawn();

        // Spawn children.
        {
            std::scoped_lock childGuard(mtxChildNodes.first);

            for (const auto& pChildNode : *mtxChildNodes.second) {
                pChildNode->spawn();
            }
        }
    }

    void Node::despawn() {
        std::scoped_lock guard(mtxSpawning);

        if (!bIsSpawned) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "an attempt was made to despawn already despawned node \"{}\", aborting this operation",
                    getName()),
                sNodeLogCategory);
            return;
        }

        // Despawn children first.
        // This despawn order is required for some nodes to work correctly.
        // With this despawn order we will not make "holes" in world's node tree
        // (i.e. when node is spawned, node's parent is not spawned but parent's parent node is spawned).
        {
            std::scoped_lock childGuard(mtxChildNodes.first);

            for (const auto& pChildNode : *mtxChildNodes.second) {
                pChildNode->despawn();
            }
        }

        // Despawn self.
        onDespawn();
        bIsSpawned = false;
        pGameInstance = nullptr; // don't allow accessing game instance or world at this point
    }

    gc<Node> Node::getParent() {
        std::scoped_lock guard(mtxParentNode.first);
        return mtxParentNode.second;
    }

    std::optional<Error>
    Node::serializeNodeTree(const std::filesystem::path& pathToFile, bool bEnableBackup) {
        lockChildren(); // make sure nothing is deleted while we are serializing
        {
            // Collect information for serialization.
            size_t iId = 0;
            const auto vNodesInfo = getInformationForSerialization(iId, {});

            // Serialize.
            const auto optionalError =
                Serializable::serialize(pathToFile, std::move(vNodesInfo), bEnableBackup);
            if (optionalError.has_value()) {
                auto err = optionalError.value();
                err.addEntry();
                return err;
            }
        }
        unlockChildren();

        return {};
    }

    std::vector<SerializableObjectInformation>
    Node::getInformationForSerialization(size_t& iId, std::optional<size_t> iParentId) {
        // Prepare information about nodes.
        // Use custom attributes for storing hierarchy information.
        std::vector<SerializableObjectInformation> vNodesInfo;

        // Add self first.
        const size_t iMyId = iId;
        SerializableObjectInformation selfInfo(this, std::to_string(iId));
        if (iParentId.has_value()) {
            selfInfo.customAttributes[sParentNodeAttributeName] = std::to_string(iParentId.value());
        }
        vNodesInfo.push_back(std::move(selfInfo));
        iId += 1;

        // Add child information.
        std::scoped_lock guard(mtxChildNodes.first);
        for (const auto& pChildNode : *mtxChildNodes.second) {
            std::ranges::move(
                pChildNode->getInformationForSerialization(iId, iMyId), std::back_inserter(vNodesInfo));
        }

        return vNodesInfo;
    }

    void Node::lockChildren() {
        mtxChildNodes.first.lock();
        for (const auto& pChildNode : *mtxChildNodes.second) {
            pChildNode->lockChildren();
        }
    }

    void Node::unlockChildren() {
        mtxChildNodes.first.unlock();
        for (const auto& pChildNode : *mtxChildNodes.second) {
            pChildNode->unlockChildren();
        }
    }

    std::variant<gc<Node>, Error> Node::deserializeNodeTree(const std::filesystem::path& pathToFile) {
        // Get all IDs from this file.
        const auto idResult = Serializable::getIdsFromFile(pathToFile);
        if (std::holds_alternative<Error>(idResult)) {
            auto err = std::get<Error>(idResult);
            err.addEntry();
            return err;
        }
        const auto ids = std::get<std::set<std::string>>(idResult);

        // Deserialize all nodes.
        const auto deserializeResult = Serializable::deserialize(pathToFile, ids);
        if (std::holds_alternative<Error>(deserializeResult)) {
            auto err = std::get<Error>(deserializeResult);
            err.addEntry();
            return err;
        }
        const auto vNodesInfo =
            std::get<std::vector<DeserializedObjectInformation>>(std::move(deserializeResult));

        // Sort all nodes by their ID.
        gc<Node> pRootNode;
        // Prepare array of pairs: Node - Parent index.
        std::vector<std::pair<Node*, std::optional<size_t>>> vNodes(vNodesInfo.size());
        for (const auto& nodeInfo : vNodesInfo) {
            // Try to cast this object to Node.
            const auto pNode = dynamic_cast<Node*>(&*nodeInfo.pObject);
            if (!pNode) [[unlikely]] {
                return Error("Deserialized object is not a node.");
            }

            // Check that this object has required attribute about parent ID.
            std::optional<size_t> iParentNodeId = {};
            const auto parentNodeAttributeIt = nodeInfo.customAttributes.find(sParentNodeAttributeName);
            if (parentNodeAttributeIt == nodeInfo.customAttributes.end()) {
                if (!pRootNode) {
                    pRootNode = gc<Node>(pNode);
                } else {
                    return Error(fmt::format(
                        "Found non root node \"{}\" that does not have a parent.", pNode->getName()));
                }
            } else {
                try {
                    iParentNodeId = std::stoull(parentNodeAttributeIt->second);
                } catch (std::exception& exception) {
                    return Error(fmt::format(
                        "Failed to convert attribute \"{}\" with value \"{}\" to integer, error: {}.",
                        sParentNodeAttributeName,
                        parentNodeAttributeIt->second,
                        exception.what()));
                }

                // Check if this parent ID is outside of out array bounds.
                if (iParentNodeId.value() >= vNodes.size()) [[unlikely]] {
                    return Error(fmt::format(
                        "Parsed parent node ID is outside of bounds: {} >= {}.",
                        iParentNodeId.value(),
                        vNodes.size()));
                }
            }

            // Try to convert this node's ID to size_t.
            size_t iNodeId = 0;
            try {
                iNodeId = std::stoull(nodeInfo.sObjectUniqueId);
            } catch (std::exception& exception) {
                return Error(fmt::format(
                    "Failed to convert ID \"{}\" to integer, error: {}.",
                    nodeInfo.sObjectUniqueId,
                    exception.what()));
            }

            // Check if this ID is outside of out array bounds.
            if (iNodeId >= vNodes.size()) [[unlikely]] {
                return Error(
                    fmt::format("Parsed ID is outside of bounds: {} >= {}.", iNodeId, vNodes.size()));
            }

            // Check if we already set a node in this index position.
            if (vNodes[iNodeId].first) [[unlikely]] {
                return Error(fmt::format("Parsed ID {} was already used by some other node.", iNodeId));
            }

            // Save node.
            vNodes[iNodeId] = {pNode, iParentNodeId};
        }

        // See if we found root node.
        if (!pRootNode) [[unlikely]] {
            return Error("Root node was not found.");
        }

        // Build hierarchy using value from attribute.
        for (const auto& node : vNodes) {
            if (node.second.has_value()) {
                vNodes[node.second.value()].first->addChildNode(gc<Node>(node.first));
            }
        }

        return pRootNode;
    }

    gc_vector<Node> Node::getChildNodes() {
        std::scoped_lock guard(mtxChildNodes.first);

        return mtxChildNodes.second;
    }

    GameInstance* Node::getGameInstance() {
        std::scoped_lock guard(mtxSpawning);

        return pGameInstance;
    }

    gc<Node> Node::getWorldRootNode() {
        std::scoped_lock guard(mtxSpawning);

        if (!pGameInstance)
            return nullptr;

        return pGameInstance->getWorldRootNode();
    }

    bool Node::isParentOf(Node* pNode) {
        std::scoped_lock guard(mtxChildNodes.first);

        for (const auto& pChildNode : *mtxChildNodes.second) {
            if (&*pChildNode == pNode) {
                return true;
            }

            const auto bIsChild = pChildNode->isParentOf(pNode);
            if (!bIsChild) {
                continue;
            } else {
                return true;
            }
        }

        return false;
    }

    bool Node::isChildOf(Node* pNode) {
        std::scoped_lock guard(mtxParentNode.first);

        // Check if have a parent.
        if (!mtxParentNode.second)
            return false;

        if (&*mtxParentNode.second == pNode) {
            return true;
        }

        return mtxParentNode.second->isChildOf(pNode);
    }

} // namespace ne
