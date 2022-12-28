#include "game/nodes/Node.h"

// Standard.
#include <ranges>

// Custom.
#include "io/Logger.h"
#include "misc/Globals.h"
#include "game/GameInstance.h"
#include "game/World.h"
#include "game/Game.h"

/** Total amount of alive nodes. */
static std::atomic<size_t> iTotalAliveNodeCount{0};

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
                        "a direct child node of \"{}\", aborting this operation",
                        pNode->getName(),
                        getName(),
                        getName()),
                    sNodeLogCategory);
                return;
            }
        }

        // Check that we don't add our parent as our child.
        if (pNode->isParentOf(this)) {
            Logger::get().error(
                fmt::format(
                    "an attempt was made to attach the \"{}\" node to the node \"{}\", "
                    "but the first node is a parent of the second node, "
                    "aborting this operation",
                    pNode->getName(),
                    getName()),
                sNodeLogCategory);
            return;
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
            pNode->notifyAboutDetachingFromParent(true);

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
        pNode->notifyAboutAttachedToNewParent(true);

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
        if (&*getWorldRootNode() == this) [[unlikely]] {
            Error error("Instead of despawning world's root node, create/replace world using GameInstance "
                        "functions, this would destroy the previous world with all nodes.");
            error.showError();
            throw std::runtime_error(error.getError());
        }

        // Check if this node is spawned.
        std::scoped_lock guard(mtxSpawning);
        if (bIsSpawned == false)
            return;

        // Detach from parent.
        std::scoped_lock parentGuard(mtxParentNode.first);
        gc<Node> pSelf;

        if (mtxParentNode.second != nullptr) {
            // Notify.
            notifyAboutDetachingFromParent(true);

            // Remove this node from parent's children array.
            std::scoped_lock parentChildGuard(mtxParentNode.second->mtxChildNodes.first);
            auto pParentChildren = &mtxParentNode.second->mtxChildNodes.second;
            bool bFound = false;
            for (auto it = (*pParentChildren)->begin(); it != (*pParentChildren)->end(); ++it) {
                if (&**it == this) {
                    pSelf = *it; // make sure we won't be deleted while being in this function
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

            // Clear parent.
            mtxParentNode.second = nullptr;
        }

        // Despawn.
        despawn();

        // Queue garbage collection.
        pSelf = nullptr;
        getGameInstance()->queueGarbageCollection({});
    }

    bool Node::isSpawned() {
        std::scoped_lock guard(mtxSpawning);
        return bIsSpawned;
    }

    World* Node::findValidWorld() {
        std::scoped_lock guard(mtxSpawning);

        if (pWorld)
            return pWorld;

        // Ask parent node for the valid game instance and world pointers.
        std::scoped_lock parentGuard(mtxParentNode.first);
        if (!mtxParentNode.second) [[unlikely]] {
            Error err(fmt::format(
                "node \"{}\" can't find a pointer to a valid world instance because "
                "there is no parent node",
                getName()));
            err.showError();
            throw std::runtime_error(err.getError());
        }

        return mtxParentNode.second->findValidWorld();
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

        // Initialize world.
        pWorld = findValidWorld();

        // Spawn self first and only then child nodes.
        // This spawn order is required for some nodes to work correctly.
        // With this spawn order we will not make "holes" in world's node tree
        // (i.e. when node is spawned, node's parent is not spawned but parent's parent node is spawned).
        bIsSpawned = true;
        onSpawn();

        // Notify world.
        pWorld->onNodeSpawned(gc<Node>(this));

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

        // Notify world.
        pWorld->onNodeDespawned(gc<Node>(this));

        // Despawn self.
        onDespawn();
        bIsSpawned = false;
        pWorld = nullptr; // don't allow accessing world at this point
    }

    gc<Node> Node::getParentNode() {
        std::scoped_lock guard(mtxParentNode.first);
        return mtxParentNode.second;
    }

    std::optional<Error>
    Node::serializeNodeTree(const std::filesystem::path& pathToFile, bool bEnableBackup) {
        lockChildren(); // make sure nothing is deleted while we are serializing
        {
            // Collect information for serialization.
            size_t iId = 0;
            auto result = getInformationForSerialization(iId, {});
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(result);
                error.addEntry();
                return error;
            }
            const auto vOriginalNodesInfo =
                std::get<std::vector<SerializableObjectInformationWithGcPointer>>(result);

            // Convert information array.
            std::vector<SerializableObjectInformation> vNodesInfo;
            for (const auto& info : vOriginalNodesInfo) {
                vNodesInfo.push_back(SerializableObjectInformation(
                    info.pObject, info.sObjectUniqueId, info.customAttributes, info.pOriginalObject));
            }

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

    std::variant<std::vector<Node::SerializableObjectInformationWithGcPointer>, Error>
    Node::getInformationForSerialization(size_t& iId, std::optional<size_t> iParentId) {
        // Prepare information about nodes.
        // Use custom attributes for storing hierarchy information.
        std::vector<SerializableObjectInformationWithGcPointer> vNodesInfo;

        // Add self first.
        const size_t iMyId = iId;

        SerializableObjectInformationWithGcPointer selfInfo(this, std::to_string(iId));

        // Add parent ID.
        if (iParentId.has_value()) {
            selfInfo.customAttributes[sParentNodeIdAttributeName] = std::to_string(iParentId.value());
        }

        // Add original object.
        bool bIncludeInformationAboutChildNodes = true;
        if (getPathDeserializedFromRelativeToRes().has_value()) {
            // This entity was deserialized from the `res` directory.
            // We should only serialize fields with changed values and additionally serialize
            // the path to the original file so that the rest
            // of the fields can be deserialized from that file.

            // Check that entity file exists.
            const auto pathToOriginalFile =
                getPathToResDirectory() / getPathDeserializedFromRelativeToRes().value().first;
            if (!std::filesystem::exists(pathToOriginalFile)) {
                const Error error(fmt::format(
                    "object of type \"{}\" has the path it was deserialized from ({}, ID {}) but this "
                    "file \"{}\" does not exist",
                    getArchetype().getName(),
                    getPathDeserializedFromRelativeToRes().value().first,
                    getPathDeserializedFromRelativeToRes().value().second,
                    pathToOriginalFile.string()));
                return error;
            }

            // Deserialize the original entity.
            auto result = deserialize<Node, gc>(
                pathToOriginalFile, getPathDeserializedFromRelativeToRes().value().second);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(result);
                error.addEntry();
                return error;
            }
            selfInfo.pDeserializedOriginalObject = std::get<gc<Node>>(result);
            selfInfo.pOriginalObject = &*selfInfo.pDeserializedOriginalObject;

            // Check if child nodes are located in the same file
            // (i.e. check if this node is a root of some external node tree).
            if (!mtxChildNodes.second->empty() &&
                isTreeDeserializedFromOneFile(getPathDeserializedFromRelativeToRes().value().first)) {
                // Don't serialize information about child nodes,
                // when referencing an external node tree, we should only
                // allow modifying the root node, thus, because only root node
                // can have changed fields, we don't include child nodes here.
                bIncludeInformationAboutChildNodes = false;
                selfInfo.customAttributes[sExternalNodeTreePathAttributeName] =
                    getPathDeserializedFromRelativeToRes().value().first;
            }
        }
        vNodesInfo.push_back(std::move(selfInfo));

        iId += 1;

        if (bIncludeInformationAboutChildNodes) {
            // Add child information.
            std::scoped_lock guard(mtxChildNodes.first);
            for (const auto& pChildNode : *mtxChildNodes.second) {
                auto result = pChildNode->getInformationForSerialization(iId, iMyId);
                if (std::holds_alternative<Error>(result)) {
                    auto error = std::get<Error>(result);
                    return error;
                }
                auto vChildArray =
                    std::get<std::vector<SerializableObjectInformationWithGcPointer>>(std::move(result));
                std::ranges::move(vChildArray, std::back_inserter(vNodesInfo));
            }
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
        auto vNodesInfo = std::get<std::vector<DeserializedObjectInformation>>(std::move(deserializeResult));

        // See if some node is external node tree.
        for (auto& nodeInfo : vNodesInfo) {
            // Try to cast this object to Node.
            if (!dynamic_cast<Node*>(&*nodeInfo.pObject)) [[unlikely]] {
                return Error("deserialized object is not a node");
            }

            auto it = nodeInfo.customAttributes.find(sExternalNodeTreePathAttributeName);
            if (it == nodeInfo.customAttributes.end())
                continue;

            // Construct path to this external node tree.
            const auto pathToExternalNodeTree =
                ProjectPaths::getDirectoryForResources(ResourceDirectory::ROOT) / it->second;
            if (!std::filesystem::exists(pathToExternalNodeTree)) {
                Error error(fmt::format(
                    "file storing external node tree \"{}\" does not exist",
                    pathToExternalNodeTree.string()));
                return error;
            }

            // Deserialize external node tree.
            auto result = deserializeNodeTree(pathToExternalNodeTree);
            if (std::holds_alternative<Error>(result)) {
                auto error = std::get<Error>(result);
                error.addEntry();
                return error;
            }
            gc<Node> pExternalRootNode = std::get<gc<Node>>(result);

            gc<Node> pNode = gc_dynamic_pointer_cast<Node>(nodeInfo.pObject);

            // Attach child nodes of this external root node to our node that has changed fields.
            const auto vChildNodes = pExternalRootNode->getChildNodes();
            for (const auto& pChildNode : *vChildNodes) {
                pNode->addChildNode(pChildNode);
            }
        }

        // Sort all nodes by their ID.
        gc<Node> pRootNode;
        // Prepare array of pairs: Node - Parent index.
        std::vector<std::pair<Node*, std::optional<size_t>>> vNodes(vNodesInfo.size());
        for (const auto& nodeInfo : vNodesInfo) {
            // Try to cast this object to Node.
            const auto pNode = dynamic_cast<Node*>(&*nodeInfo.pObject);
            if (!pNode) [[unlikely]] {
                return Error("deserialized object is not a node");
            }

            // Check that this object has required attribute about parent ID.
            std::optional<size_t> iParentNodeId = {};
            const auto parentNodeAttributeIt = nodeInfo.customAttributes.find(sParentNodeIdAttributeName);
            if (parentNodeAttributeIt == nodeInfo.customAttributes.end()) {
                if (!pRootNode) {
                    pRootNode = gc<Node>(pNode);
                } else {
                    return Error(fmt::format(
                        "found non root node \"{}\" that does not have a parent", pNode->getName()));
                }
            } else {
                try {
                    iParentNodeId = std::stoull(parentNodeAttributeIt->second);
                } catch (std::exception& exception) {
                    return Error(fmt::format(
                        "failed to convert attribute \"{}\" with value \"{}\" to integer, error: {}",
                        sParentNodeIdAttributeName,
                        parentNodeAttributeIt->second,
                        exception.what()));
                }

                // Check if this parent ID is outside of out array bounds.
                if (iParentNodeId.value() >= vNodes.size()) [[unlikely]] {
                    return Error(fmt::format(
                        "parsed parent node ID is outside of bounds: {} >= {}",
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
                    "failed to convert ID \"{}\" to integer, error: {}",
                    nodeInfo.sObjectUniqueId,
                    exception.what()));
            }

            // Check if this ID is outside of out array bounds.
            if (iNodeId >= vNodes.size()) [[unlikely]] {
                return Error(fmt::format("parsed ID is outside of bounds: {} >= {}", iNodeId, vNodes.size()));
            }

            // Check if we already set a node in this index position.
            if (vNodes[iNodeId].first) [[unlikely]] {
                return Error(fmt::format("parsed ID {} was already used by some other node", iNodeId));
            }

            // Save node.
            vNodes[iNodeId] = {pNode, iParentNodeId};
        }

        // See if we found root node.
        if (!pRootNode) [[unlikely]] {
            return Error("root node was not found");
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

    GameInstance* Node::getGameInstance() { return Game::get()->getGameInstance(); }

    gc<Node> Node::getWorldRootNode() {
        std::scoped_lock guard(mtxSpawning);

        if (pWorld == nullptr)
            return nullptr;

        return pWorld->getRootNode();
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

    void Node::notifyAboutAttachedToNewParent(bool bThisNodeBeingAttached) {
        std::scoped_lock guard(mtxParentNode.first, mtxChildNodes.first);

        onAfterAttachedToNewParent(bThisNodeBeingAttached);

        for (const auto& pChildNode : *mtxChildNodes.second) {
            pChildNode->notifyAboutAttachedToNewParent(false);
        }
    }

    void Node::notifyAboutDetachingFromParent(bool bThisNodeBeingDetached) {
        std::scoped_lock guard(mtxParentNode.first, mtxChildNodes.first);

        onBeforeDetachedFromParent(bThisNodeBeingDetached);

        for (const auto& pChildNode : *mtxChildNodes.second) {
            pChildNode->notifyAboutDetachingFromParent(false);
        }
    }

    bool Node::isTreeDeserializedFromOneFile(const std::string& sPathRelativeToRes) {
        if (!getPathDeserializedFromRelativeToRes().has_value()) {
            return false;
        }

        if (getPathDeserializedFromRelativeToRes().value().first != sPathRelativeToRes) {
            return false;
        }

        bool bPathEqual = true;
        lockChildren();
        {
            for (const auto& pChildNode : *mtxChildNodes.second) {
                if (!pChildNode->isTreeDeserializedFromOneFile(sPathRelativeToRes)) {
                    bPathEqual = false;
                    break;
                }
            }
        }
        unlockChildren();

        return bPathEqual;
    }

    bool Node::isCalledEveryFrame() const { return bIsCalledEveryFrame; }

    void Node::setIsCalledEveryFrame(bool bEnable) {
        std::scoped_lock guard(mtxSpawning);
        if (bIsSpawned) {
            Error error("this function should not be called while the node is spawned");
            error.showError();
            throw std::runtime_error(error.getError());
        }

        bIsCalledEveryFrame = bEnable;
    }

    void Node::setTickGroup(TickGroup tickGroup) {
        std::scoped_lock guard(mtxSpawning);
        if (bIsSpawned) {
            Error error("this function should not be called while the node is spawned");
            error.showError();
            throw std::runtime_error(error.getError());
        }

        this->tickGroup = tickGroup;
    }

    TickGroup Node::getTickGroup() const { return tickGroup; }

    void Node::setReceiveInput(bool bEnable) {
        std::scoped_lock guard(mtxSpawning);
        if (bIsSpawned) {
            Error error("this function should not be called while the node is spawned");
            error.showError();
            throw std::runtime_error(error.getError());
        }

        this->bReceiveInput = bEnable;
    }

    bool Node::receivesInput() const { return bReceiveInput; }

    void Node::onInputActionEvent(
        const std::string& sActionName, KeyboardModifiers modifiers, bool bIsPressedDown) {
        std::scoped_lock guard(mtxBindedActionEvents.first);

        const auto it = mtxBindedActionEvents.second.find(sActionName);
        if (it == mtxBindedActionEvents.second.end())
            return;

        it->second(modifiers, bIsPressedDown);
    }

    void Node::onInputAxisEvent(const std::string& sAxisName, KeyboardModifiers modifiers, float input) {
        std::scoped_lock guard(mtxBindedAxisEvents.first);

        const auto it = mtxBindedAxisEvents.second.find(sAxisName);
        if (it == mtxBindedAxisEvents.second.end())
            return;

        it->second(modifiers, input);
    }

    std::pair<
        std::recursive_mutex,
        std::unordered_map<std::string, std::function<void(KeyboardModifiers, bool)>>>*
    Node::getActionEventBindings() {
        return &mtxBindedActionEvents;
    }

    std::pair<
        std::recursive_mutex,
        std::unordered_map<std::string, std::function<void(KeyboardModifiers, float)>>>*
    Node::getAxisEventBindings() {
        return &mtxBindedAxisEvents;
    }

} // namespace ne
