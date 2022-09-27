#include "game/nodes/Node.h"

// Custom.
#include "io/Logger.h"

#if defined(DEBUG)
/** Total amount of created nodes. */
static std::atomic<size_t> iTotalNodeCount{0};
#endif

namespace ne {
    Node::Node() : Node("Node") {}

    Node::Node(const std::string& sName) {
        this->sName = sName;
        mtxParentNode.second = nullptr;
        mtxIsSpawned.second = false;

#if defined(DEBUG)
        const size_t iNodeCount = iTotalNodeCount.fetch_add(1);
        Logger::get().info(
            fmt::format(
                "[DEBUG ONLY] constructor for node \"{}\" is called (alive nodes now: {})",
                sName,
                iNodeCount),
            sNodeLogCategory);
#endif
    }

    Node::~Node() {
        std::scoped_lock guard(mtxIsSpawned.first);
        if (mtxIsSpawned.second) {
            despawn();
        }

#if defined(DEBUG)
        const size_t iNodesLeft = iTotalNodeCount.fetch_sub(1);
        Logger::get().info(
            fmt::format(
                "[DEBUG ONLY] destructor for node \"{}\" is called (alive nodes left: {})",
                sName,
                iNodesLeft),
            sNodeLogCategory);
#endif
    }

    void Node::setName(const std::string& sName) { this->sName = sName; }

    std::string Node::getName() const { return sName; }

    void Node::addChildNode(std::shared_ptr<Node> pNode) {
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

        // Check if this node is this.
        if (pNode.get() == this) [[unlikely]] {
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
        for (const auto& pChildNode : mtxChildNodes.second) {
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
            Error err(fmt::format(
                "Node \"{}\" attempts to change its parent node from node \"{}\" to node \"{}\". "
                "Changing node's parent is not supported yet. If you need to attach your node to another "
                "parent, right now you have 2 ways of implementing this:\n"
                "1. Create a new duplicate node and attach it instead, remove old node.\n"
                "2. Have 2 nodes, one is attached but hidden, while other is visible. When you "
                "need to change node's parent, hide old node and show the hidden (attached) one.",
                pNode->getName(),
                pNode->mtxParentNode.second->getName(),
                getName()));
            err.showError();
            throw std::runtime_error(err.getError());
            // TODO: show error until we will find a way to solve cyclic reference that may
            // TODO: occur when changing parent, example: node A is parent of B and B is parent
            // TODO: if C, B additionally stores a shared pointer to C but C decided to change
            // TODO: parent from B to A, after changing parent we have A - C - B hierarchy but B
            // TODO: stores a shared pointer to C and C stores shared pointer to B (because nodes
            // TODO: (store shared pointers to children) - cyclic reference.
            //            Logger::get().warn(
            //                fmt::format(
            //                    "node \"{}\" is changing its parent node from node \"{}\" to node \"{}\", "
            //                    "changing parent node is dangerous and can cause cyclic references, "
            //                    "make sure you know what you are doing",
            //                    pNode->getName(),
            //                    pNode->mtxParentNode.second->getName(),
            //                    getName()),
            //                sNodeLogCategory);

            //            // Change node parent.
            //            pNode->onBeforeDetachedFromNode(pNode->mtxParentNode.second);

            //            pNode->mtxParentNode.second = this;
            //            mtxChildNodes.second.push_back(pNode);

            //            pNode->onAfterAttachedToNode(mtxParentNode.second);
        }

        // don't unlock node's parent lock here yet, still doing some logic based on new parent

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
        std::scoped_lock spawnGuard(mtxIsSpawned.first);
        if (mtxIsSpawned.second == false)
            return;

        // Detach from parent.
        std::scoped_lock parentGuard(mtxParentNode.first);
        std::shared_ptr<Node> pSelf;

        if (mtxParentNode.second != nullptr) {
            // Remove this node from parent's children array.
            std::scoped_lock parentChildGuard(mtxParentNode.second->mtxChildNodes.first);
            auto pParentChildren = &mtxParentNode.second->mtxChildNodes.second;
            for (auto it = pParentChildren->begin(); it != pParentChildren->end(); ++it) {
                if (it->get() == this) {
                    pSelf = *it; // keep a shared pointer to self until we are not finished
                    pParentChildren->erase(it);
                    break;
                }
            }

            // Remove parent.
            mtxParentNode.second = nullptr;
        }

        // Despawn.
        despawn();
    }

    bool Node::isSpawned() {
        std::scoped_lock guard(mtxIsSpawned.first);
        return mtxIsSpawned.second;
    }

    void Node::spawn() {
        std::scoped_lock guard(mtxIsSpawned.first);

        if (mtxIsSpawned.second) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "an attempt was made to spawn already spawned node \"{}\", aborting this operation",
                    getName()),
                sNodeLogCategory);
            return;
        }

        // Spawn self first and only then child nodes.
        // This spawn order is required for some nodes to work correctly.
        // With this spawn order we will not make "holes" in world's node tree
        // (i.e. when node is spawned, node's parent is not spawned but parent's parent node is spawned).
        mtxIsSpawned.second = true;
        onSpawn();

        // Spawn children.
        {
            std::scoped_lock childGuard(mtxChildNodes.first);

            for (const auto& pChildNode : mtxChildNodes.second) {
                pChildNode->spawn();
            }
        }
    }

    void Node::despawn() {
        std::scoped_lock guard(mtxIsSpawned.first);

        if (!mtxIsSpawned.second) [[unlikely]] {
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

            for (const auto& pChildNode : mtxChildNodes.second) {
                pChildNode->despawn();
            }
        }

        // Despawn self.
        onDespawn();
        mtxIsSpawned.second = false;
    }

    Node* Node::getParent() {
        std::scoped_lock guard(mtxParentNode.first);
        return mtxParentNode.second;
    }

} // namespace ne
