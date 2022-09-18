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
         * Returns a copy of the array of child nodes.
         *
         * @return A copy of the array of child nodes.
         */
        std::vector<std::shared_ptr<Node>> getChildNodes();

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
         * Called when this node was not spawned and was attached to a parent node that is spawned
         * to execute custom spawn logic.
         *
         * @remark This node will be marked as spawned before this function is called.
         * @remark This function is called only after all child nodes were spawned.
         *
         * @warning It's better to call parent's version first (before executing your logic).
         */
        virtual void onSpawn(){};

        /**
         * Called before this node is despawned from the world to execute custom despawn logic.
         *
         * @remark This node will be marked as despawned after this function is called.
         * @remark This function is called before child nodes are despawned.
         *
         * @warning It's better to call parent's version first (before executing your logic).
         */
        virtual void onDespawn(){};

    private:
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
} // namespace )

File_Node_GENERATED
