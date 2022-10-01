﻿#pragma once

// STL.
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

// Custom.
#include "io/Serializable.h"
#include "misc/GC.hpp"

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
         * Returns the total amount of currently alive (allocated) nodes.
         *
         * @return Number of alive nodes right now.
         */
        static size_t getAliveNodeCount();

        /**
         * Creates a node with a default name.
         */
        Node();

        /**
         * Creates a node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        Node(const std::string& sNodeName);

        Node(const Node&) = delete;
        Node& operator=(const Node&) = delete;
        Node(Node&&) = delete;
        Node& operator=(Node&&) = delete;

        /** Logs destruction in debug builds. */
        virtual ~Node() override;

        /**
         * Deserializes a node and all its child nodes (hierarchy information) from a file.
         *
         * @param pathToFile    File to read a node tree from. The ".toml" extension will be added
         * automatically if not specified in the path.
         *
         * @return Error if something went wrong, otherwise pointer to the root node.
         */
        static std::variant<gc<Node>, Error> deserializeNodeTree(const std::filesystem::path& pathToFile);

        /**
         * Sets node's name.
         *
         * @param sName New name of this node.
         */
        void setName(const std::string& sName);

        /**
         * Detaches this node from the parent and despawns this node and
         * all of its child nodes.
         *
         * The node and its child nodes are not guaranteed to be deleted after this
         * function is finished. Deletion is handled automatically by gc pointers.
         */
        void detachFromParentAndDespawn();

        /**
         * Attaches a node as a child of this node.
         *
         * @param pNode Node to attach as a child.
         */
        void addChildNode(gc<Node> pNode);

        /**
         * Returns node's name.
         *
         * @return Node name.
         */
        std::string getName() const;

        /**
         * Returns whether this node is spawned in the world or not.
         *
         * @return Whether this node is spawned in the world or not.
         */
        bool isSpawned();

        /**
         * Returns parent node if this node.
         *
         * @return Do not delete returned pointer.
         * nullptr if there is no parent node, otherwise valid pointer.
         */
        gc<Node> getParent();

        /**
         * Returns a copy of the array of child nodes.
         *
         * @return Copy of the array of child nodes.
         */
        gc_vector<Node> getChildNodes();

        /**
         * Serializes the node and all child nodes (hierarchy information will also be saved) into a file.
         * Node tree can later be deserialized using @ref deserializeNodeTree.
         *
         * @param pathToFile    File to write the node tree to. The ".toml" extension will be added
         * automatically if not specified in the path. If the specified file already exists it will be
         * overwritten.
         * @param bEnableBackup If 'true' will also use a backup (copy) file. @ref deserializeNodeTree can use
         * backup file if the original file does not exist. Generally you want to use
         * a backup file if you are saving important information, such as player progress,
         * other cases such as player game settings and etc. usually do not need a backup but
         * you can use it if you want.
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field.
         */
        std::optional<Error> serializeNodeTree(const std::filesystem::path& pathToFile, bool bEnableBackup);

    protected:
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
        requires std::derived_from<NodeType, Node> gc<Node>
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
        requires std::derived_from<NodeType, Node> gc<Node>
        getChildNodeOfType(const std::string& sChildNodeName = "");

    private:
        // World is able to spawn root node.
        friend class World;

        /** Calls @ref onSpawn on this node and all of its child nodes. */
        void spawn();

        /** Calls @ref onDespawn on this node and all of its child nodes. */
        void despawn();

        /**
         * Collects and returns information for serialization for self and all child nodes.
         *
         * @param iId       ID for serialization to use (will be incremented).
         * @param iParentId Parent's serialization ID (if this node has a parent and it will also
         * be serialized).
         *
         * @return Array of collected information that can be serialized.
         */
        std::vector<SerializableObjectInformation>
        getInformationForSerialization(size_t& iId, std::optional<size_t> iParentId);

        /**
         * Locks @ref mtxChildNodes mutex for self and recursively for all children.
         *
         * After a node with children was locked this makes the whole node tree to be
         * frozen (hierarchy can't be changed).
         *
         * Use @ref unlockChildren for unlocking the tree.
         */
        void lockChildren();

        /**
         * Unlocks @ref mtxChildNodes mutex for self and recursively for all children.
         *
         * After a node with children was unlocked this makes the whole node tree to be
         * unfrozen (hierarchy can be changed as usual).
         */
        void unlockChildren();

        /** Node name. */
        NEPROPERTY()
        std::string sName;

        /** Attached child nodes. Should be used under the mutex when changing children. */
        std::pair<std::recursive_mutex, gc_vector<Node>> mtxChildNodes;

        /**
         * Do not delete. Attached parent node.
         */
        std::pair<std::recursive_mutex, gc<Node>> mtxParentNode;

        /**
         * Whether this node is spawned in the world or not.
         * Should be used under the mutex when spawning/despawning.
         */
        std::pair<std::recursive_mutex, bool> mtxIsSpawned;

        /** Name of the attribute we use to serialize information about parent node. */
        static inline const auto sParentNodeAttributeName = "parent_node";

        /** Name of the category used for logging. */
        static inline const auto sNodeLogCategory = "Node";

        ne_Node_GENERATED
    };

    template <typename NodeType>
    requires std::derived_from<NodeType, Node> gc<Node> Node::getParentNodeOfType(
        const std::string& sParentNodeName) {
        std::scoped_lock guard(mtxParentNode.first);

        // Check if have parent.
        if (!mtxParentNode.second)
            return nullptr;

        // Check parent's type and optionally name.
        if (dynamic_cast<NodeType>(&*mtxParentNode.second) &&
            (sParentNodeName.empty() || mtxParentNode.second->getName() == sParentNodeName)) {
            return mtxParentNode.second;
        }

        return mtxParentNode.second->getParentNodeOfType<NodeType>(sParentNodeName);
    }

    template <typename NodeType>
    requires std::derived_from<NodeType, Node> gc<Node> Node::getChildNodeOfType(
        const std::string& sChildNodeName) {
        std::scoped_lock guard(mtxChildNodes.first);

        for (const auto& pChildNode : *mtxChildNodes.second) {
            if (dynamic_cast<NodeType>(&*pChildNode) &&
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
