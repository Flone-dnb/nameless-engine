#pragma once

// STL.
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

// Custom.
#include "io/Serializable.h"

#include "Node.generated.h"

namespace ne NENAMESPACE() {
    /**
     * Base class for nodes, allows being spawned in the world, attaching child nodes
     * or being attached to some parent node.
     *
     * @warning If changing the class name class ID (of this class) will change and will make all previously
     * serialized instances of this class reference old (invalid) class ID. Include a backwards compatibility
     * in this case.
     */
    class NECLASS(Guid("2a721c37-3c22-450c-8dad-7b6985cbbd61")) Node : public Serializable {
    public:
        /**
         * Creates a node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        Node(const std::string& sNodeName);

        Node() = delete;
        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = delete;
        Node& operator=(Node&&) = delete;

        /** Logs destruction in debug builds. */
        virtual ~Node() override;

        /**
         * Sets node's name.
         *
         * @param sName New name of this node.
         */
        NEFUNCTION()
        void setName(const std::string& sName);

        /**
         * Returns node's name.
         *
         * @return Node name.
         */
        NEFUNCTION()
        std::string getName() const;

        /**
         * Returns whether this node is spawned in the world or not.
         *
         * @return Whether this node is spawned in the world or not.
         */
        NEFUNCTION()
        bool isSpawned();

        /**
         * Returns parent node if this node.
         *
         * @return Do not delete returned pointer.
         * nullptr if there is no parent node, otherwise valid pointer.
         */
        NEFUNCTION()
        Node* getParent();

        /**
         * Detaches this node from the parent and despawns this node and
         * all of its child nodes.
         *
         * The node and its child nodes are not guaranteed to be deleted after this
         * function is finished. Deletion is handled automatically by shared pointers
         * (when nobody will reference a node). If you consider the node as no longer needed
         * you can clear any shared pointers to this node that you hold. Once the world
         * is changed (world's root node changed) old root node will be deleted and so
         * references to all nodes in the world should be automatically cleaned up which
         * should cause all nodes to be deleted, unless you are holding a shared pointer to a
         * node outside of the Node class (in IGameInstance for example or in some other class).
         */
        NEFUNCTION()
        void detachFromParentAndDespawn();

        /**
         * Attaches a node as a child of this node.
         *
         * @remark If attached node had a parent it will be changed.
         *
         * @param pNode Node to attach as a child.
         */
        void addChildNode(std::shared_ptr<Node> pNode);

    protected:
        /**
         * Called right before this node will be detached from its current parent node.
         *
         * @remark Not called if node has no parent.
         *
         * @param pDetachingFromThis Current parent node that we will be detached from.
         *
         * @warning It's better to call parent's version first (before executing your logic).
         */
        virtual void onBeforeDetachedFromNode(Node* pDetachingFromThis){};

        /**
         * Called right after this node was attached to a new parent node.
         *
         * @param pAttachedToThis New parent node that we are attached to.
         *
         * @warning It's better to call parent's version first (before executing your logic).
         */
        virtual void onAfterAttachedToNode(Node* pAttachedToThis){};

        /**
         * Called when this node was not spawned and it was attached to a parent node that is spawned
         * to execute custom spawn logic.
         *
         * @remark This node will be marked as spawned before this function is called.
         * @remark This function is called before any of the child nodes are spawned.
         *
         * @warning It's better to call parent's version first (before executing your logic).
         */
        virtual void onSpawn(){};

        /**
         * Called before this node is despawned from the world to execute custom despawn logic.
         *
         * @remark This node will be marked as despawned after this function is called.
         * @remark This function is called after all child nodes were despawned.
         * @remark If node's destructor is called but node is still spawned it will be despawned.
         *
         * @warning It's better to call parent's version first (before executing your logic).
         */
        virtual void onDespawn(){};

        /**
         * Goes up the parent node chain (up to the world's root node if needed) to find
         * a first node that matches the specified node type and optionally node name.
         *
         * Template parameter NodeType specifies node type to look for. Note that
         * this means that we will use dynamic_cast to determine whether the node matches
         * the specified type or not. So if you are looking for a node with the type `Node`
         * this means that every node will match the type.
         *
         * @param sParentNodeName If not empty, nodes that match the specified node type will
         * also be checked to see if their name exactly matches the specified name.
         *
         * @return nullptr if not found, otherwise a non owning pointer to the node.
         * Do not delete returned pointer.
         */
        template <typename NodeType>
        requires std::derived_from<NodeType, Node> Node*
        getParentNodeOfType(const std::string& sParentNodeName = "");

        /**
         * Goes down the child node chain to find a first node that matches the specified node type and
         * optionally node name.
         *
         * Template parameter NodeType specifies node type to look for. Note that
         * this means that we will use dynamic_cast to determine whether the node matches
         * the specified type or not. So if you are looking for a node with the type `Node`
         * this means that every node will match the type.
         *
         * @param sChildNodeName If not empty, nodes that match the specified node type will
         * also be checked to see if their name exactly matches the specified name.
         *
         * @return nullptr if not found, otherwise a valid pointer to the node.
         */
        template <typename NodeType>
        requires std::derived_from<NodeType, Node> std::shared_ptr<Node>
        getChildNodeOfType(const std::string& sChildNodeName = "");

    private:
        // World is able to spawn root node.
        friend class World;

        /** Calls @ref onSpawn on this node and all of its child nodes. */
        void spawn();

        /** Calls @ref onDespawn on this node and all of its child nodes. */
        void despawn();

        /** Node name. */
        NEPROPERTY()
        std::string sName;

        /** Attached child nodes. Should be used under the mutex when changing children. */
        std::pair<std::recursive_mutex, std::vector<std::shared_ptr<Node>>> mtxChildNodes;

        /**
         * Do not delete. Attached parent node. Using a raw pointer because it's a non-owning reference,
         * and parent holds a shared_ptr to us. Should be used under the mutex when changing parent.
         */
        std::pair<std::recursive_mutex, Node*> mtxParentNode;

        /**
         * Whether this node is spawned in the world or not.
         * Should be used under the mutex when spawning/despawning.
         */
        std::pair<std::recursive_mutex, bool> mtxIsSpawned;

        /** Name of the category used for logging. */
        static inline const auto sNodeLogCategory = "Node";

        ne_Node_GENERATED
    };

    template <typename NodeType>
    requires std::derived_from<NodeType, Node> Node* Node::getParentNodeOfType(
        const std::string& sParentNodeName) {
        std::scoped_lock guard(mtxParentNode.first);

        // Check if have parent.
        if (!mtxParentNode.second)
            return nullptr;

        // Check parent's type and optionally name.
        if (dynamic_cast<NodeType>(mtxParentNode.second) &&
            (sParentNodeName.empty() || mtxParentNode.second->getName() == sParentNodeName)) {
            return mtxParentNode.second;
        }

        return mtxParentNode.second->getParentNodeOfType<NodeType>(sParentNodeName);
    }

    template <typename NodeType>
    requires std::derived_from<NodeType, Node> std::shared_ptr<Node> Node::getChildNodeOfType(
        const std::string& sChildNodeName) {
        std::scoped_lock guard(mtxChildNodes.first);

        for (const auto& pChildNode : mtxChildNodes.second) {
            if (dynamic_cast<NodeType>(pChildNode.get()) &&
                (sChildNodeName.empty() || pChildNode->getName() == sChildNodeName)) {
                return pChildNode;
            }

            const auto pNode = pChildNode->getChildNodeOfType<NodeType>(sChildNodeName);
            if (!pNode) {
                continue;
            } else {
                return pNode;
            }
        }

        return nullptr;
    }
} // namespace )

File_Node_GENERATED
