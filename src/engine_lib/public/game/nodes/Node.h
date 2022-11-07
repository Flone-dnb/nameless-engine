#pragma once

// STL.
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>

// Custom.
#include "io/Serializable.h"
#include "misc/GC.hpp"

#include "Node.generated.h"

namespace ne RNAMESPACE() {
    class GameInstance;

    /**
     * Base class for nodes, allows being spawned in the world, attaching child nodes
     * or being attached to some parent node.
     *
     * @warning If changing the class name class ID (of this class) will change and will make all previously
     * serialized instances of this class reference old (invalid) class ID. Include a backwards compatibility
     * in this case.
     */
    class RCLASS(Guid("2a721c37-3c22-450c-8dad-7b6985cbbd61")) Node : public Serializable {
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
         * function is finished. Deletion is handled automatically by `gc` pointers.
         */
        void detachFromParentAndDespawn();

        /**
         * Attaches a node as a child of this node.
         *
         * If the specified node already has a parent it will change its parent to be
         * a child of this node. This way you can change to which node you are attached.
         *
         * @param pNode Node to attach as a child. If the specified node is a parent of `this` node the
         * operation will fail and log an error.
         */
        void addChildNode(gc<Node> pNode);

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
         * @remark Custom attributes, like in Serializable::serialize, are not available here
         * because they are used internally to store hierarchy and other information.
         *
         * @return Error if something went wrong, for example when found an unsupported for
         * serialization reflected field.
         */
        std::optional<Error> serializeNodeTree(const std::filesystem::path& pathToFile, bool bEnableBackup);

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
         * Checks if the specified node is a child of this node (somewhere in the child hierarchy,
         * not only as a direct child node).
         *
         * @param pNode Node to check.
         *
         * @return `true` if the specified node was found as a child of this node, `false` otherwise.
         */
        bool isParentOf(Node* pNode);

        /**
         * Checks if the specified node is a parent of this node (somewhere in the parent hierarchy,
         * not only as a direct parent node).
         *
         * @param pNode Node to check.
         *
         * @return `true` if the specified node was found as a parent of this node, `false` otherwise.
         */
        bool isChildOf(Node* pNode);

        /**
         * Returns world's root node.
         *
         * @return nullptr if this node is not spawned or was despawned (always check
         * returned pointer before doing something), otherwise valid pointer.
         */
        gc<Node> getWorldRootNode();

        /**
         * Returns parent node if this node.
         *
         * @return nullptr if there is no parent node, otherwise valid pointer.
         */
        gc<Node> getParentNode();

        /**
         * Returns a copy of the array of child nodes.
         *
         * @return Copy of the array of child nodes.
         */
        gc_vector<Node> getChildNodes();

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
        requires std::derived_from<NodeType, Node> gc<NodeType>
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
        requires std::derived_from<NodeType, Node> gc<NodeType>
        getChildNodeOfType(const std::string& sChildNodeName = "");

        /**
         * Returns game instance that the world, in which the node is spawned, is using.
         *
         * @return nullptr if the node is not spawned or was despawned (always check this pointer
         * before doing something), otherwise pointer to game instance that this world is using.
         * Do not delete returned pointer.
         */
        GameInstance* getGameInstance();

        /**
         * Returns whether the @ref onBeforeNewFrame should be called each frame or not.
         *
         * @return Whether the @ref onBeforeNewFrame should be called each frame or not.
         */
        bool isCalledEveryFrame();

    protected:
        // Game will propagate functions to all nodes in the world such as onBeforeNewFrame.
        friend class Game;
        // World is able to spawn root node.
        friend class World;

        /**
         * Called before a new frame is rendered.
         *
         * @remark This function is disabled by default, use @ref setIsCalledEveryFrame to enable it.
         *
         * @warning It's recommended to call parent's version first (before executing your logic).
         *
         * @param fTimeSincePrevCallInSec Time in seconds that has passed since the last call
         * to this function.
         */
        virtual void onBeforeNewFrame(float fTimeSincePrevCallInSec) {}

        /**
         * Called when this node was not spawned and it was attached to a parent node that is spawned
         * to execute custom spawn logic.
         *
         * @remark This node will be marked as spawned before this function is called.
         * @remark This function is called before any of the child nodes are spawned.
         *
         * @warning It's recommended to call parent's version first (before executing your logic).
         */
        virtual void onSpawn() {}

        /**
         * Called before this node is despawned from the world to execute custom despawn logic.
         *
         * @remark This node will be marked as despawned after this function is called.
         * @remark This function is called after all child nodes were despawned.
         * @remark If node's destructor is called but node is still spawned it will be despawned.
         *
         * @warning It's recommended to call parent's version first (before executing your logic).
         */
        virtual void onDespawn() {}

        /**
         * Called before this node or one of the node's parents (in the parent hierarchy)
         * are about to be detached from a node.
         *
         * @warning It's recommended to call parent's version first (before executing your logic).
         */
        virtual void onBeforeDetachedFromParent() {}

        /**
         * Called after this node or one of the node's parents (in the parent hierarchy)
         * were attached to a new parent node (i.e. this node or any of its parents were attached
         * to a new node).
         *
         * @warning It's recommended to call parent's version first (before executing your logic).
         */
        virtual void onAfterAttachedToNewParent() {}

        /**
         * Determines if the @ref onBeforeNewFrame should be called each frame or not
         * (disabled by default).
         *
         * @remark Typically you should call this function in your node's constructor to determine
         * if this node should "tick" each frame or not.
         *
         * @warning Calling this function while the node is spawned will cause an error to be shown.
         *
         * @param bEnable `true` to enable @ref onBeforeNewFrame, `false` to disable.
         */
        void setIsCalledEveryFrame(bool bEnable);

        /** Mutex that will be used when spawning/despawning node (i.e. when changing @ref bIsSpawned). */
        std::recursive_mutex mtxSpawning;

    private:
        /** Small helper struct to temporary hold a GC pointer for @ref serializeNodeTree. */
        struct SerializableObjectInformationWithGcPointer : public SerializableObjectInformation {
            /**
             * Initialized object information for serialization.
             *
             * @param pObject          Object to serialize.
             * @param sObjectUniqueId  Object's unique ID. Don't use dots in IDs.
             * @param customAttributes Optional. Pairs of values to serialize with this object.
             * @param pOriginalObject  Optional. Use if the object was previously deserialized and
             * you now want to only serialize changed fields of this object and additionally store
             * the path to the original file (to deserialize unchanged fields).
             * @param pDeserializedOriginalObject Optional. GC pointer to original object.
             */
            SerializableObjectInformationWithGcPointer(
                Serializable* pObject,
                const std::string& sObjectUniqueId,
                const std::unordered_map<std::string, std::string>& customAttributes = {},
                Serializable* pOriginalObject = nullptr,
                gc<Node> pDeserializedOriginalObject = nullptr)
                : SerializableObjectInformation(pObject, sObjectUniqueId, customAttributes, pOriginalObject) {
                this->pDeserializedOriginalObject = pDeserializedOriginalObject;
            }

            /** GC pointer to the deserialized original object. */
            gc<Node> pDeserializedOriginalObject = nullptr;
        };

        /** Calls @ref onSpawn on this node and all of its child nodes. */
        void spawn();

        /** Calls @ref onDespawn on this node and all of its child nodes. */
        void despawn();

        /** Calls @ref onAfterAttachedToNewParent on this node and all of its child nodes. */
        void notifyAboutAttachedToNewParent();

        /** Calls @ref onBeforeDetachedFromParent on this node and all of its child nodes. */
        void notifyAboutDetachingFromParent();

        /**
         * Checks if this node has a valid game instance and world pointers and if not
         * asks this node's parent and goes up the node hierarchy
         * up to the root node if needed to find valid pointers to game instance and world.
         *
         * @return Valid game instance and world pointers.
         */
        std::pair<GameInstance*, World*> findValidGameInstanceAndWorld();

        /**
         * Collects and returns information for serialization for self and all child nodes.
         *
         * @param iId       ID for serialization to use (will be incremented).
         * @param iParentId Parent's serialization ID (if this node has a parent and it will also
         * be serialized).
         *
         * @return Error if something went wrong, otherwise an array of collected information
         * that can be serialized.
         */
        std::variant<std::vector<SerializableObjectInformationWithGcPointer>, Error>
        getInformationForSerialization(size_t& iId, std::optional<size_t> iParentId);

        /**
         * Checks if this node and all child nodes were deserialized from the same file
         * (i.e. checks if this node tree is located in one file).
         *
         * @param sPathRelativeToRes Path relative to the `res` directory to the file to check,
         * example: `game/test.toml`.
         *
         * @return `false` if this node or some child node(s) were deserialized from other file
         * or if some nodes we not deserialized previously, otherwise `true`.
         */
        bool isTreeDeserializedFromOneFile(const std::string& sPathRelativeToRes);

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
        RPROPERTY()
        std::string sName;

        /** Attached child nodes. Should be used under the mutex when changing children. */
        std::pair<std::recursive_mutex, gc_vector<Node>> mtxChildNodes;

        /**
         * Attached parent node.
         */
        std::pair<std::recursive_mutex, gc<Node>> mtxParentNode;

        /**
         * Do not delete this pointer. Game instance object that is used for this world.
         *
         * @warning Will be initialized after the node is spawned and reset when despawned.
         */
        GameInstance* pGameInstance = nullptr;

        /**
         * Do not delete this pointer. World object that owns this node.
         *
         * @warning Will be initialized after the node is spawned and reset when despawned.
         */
        World* pWorld = nullptr;

        /**
         * Whether this node is spawned in the world or not.
         * Should be used with @ref mtxSpawning when spawning/despawning.
         */
        bool bIsSpawned = false;

        /** Determines if the @ref onBeforeNewFrame should be called each frame or not. */
        bool bIsCalledEveryFrame = false;

        /** Name of the attribute we use to serialize information about parent node. */
        static inline const auto sParentNodeIdAttributeName = "parent_node_id";

        /** Name of the attribute we use to store a path to an external node tree. */
        static inline const auto sExternalNodeTreePathAttributeName =
            "external_node_tree_path_relative_to_res";

        /** Name of the category used for logging. */
        static inline const auto sNodeLogCategory = "Node";

        ne_Node_GENERATED
    };

    template <typename NodeType>
    requires std::derived_from<NodeType, Node> gc<NodeType> Node::getParentNodeOfType(
        const std::string& sParentNodeName) {
        std::scoped_lock guard(mtxParentNode.first);

        // Check if have a parent.
        if (!mtxParentNode.second)
            return nullptr;

        // Check parent's type and optionally name.
        if (dynamic_cast<NodeType*>(&*mtxParentNode.second) &&
            (sParentNodeName.empty() || mtxParentNode.second->getName() == sParentNodeName)) {
            return gc_dynamic_pointer_cast<NodeType>(mtxParentNode.second);
        }

        return mtxParentNode.second->getParentNodeOfType<NodeType>(sParentNodeName);
    }

    template <typename NodeType>
    requires std::derived_from<NodeType, Node> gc<NodeType> Node::getChildNodeOfType(
        const std::string& sChildNodeName) {
        std::scoped_lock guard(mtxChildNodes.first);

        for (auto& pChildNode : *mtxChildNodes.second) {
            if (dynamic_cast<NodeType*>(&*pChildNode) &&
                (sChildNodeName.empty() || pChildNode->getName() == sChildNodeName)) {
                return gc_dynamic_pointer_cast<NodeType>(pChildNode);
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
