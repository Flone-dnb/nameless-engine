#pragma once

// Standard.
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>

// Custom.
#include "io/Serializable.h"
#include "misc/GC.hpp"
#include "input/KeyboardKey.hpp"
#include "game/callbacks/NodeNotificationBroadcaster.hpp"

#include "Node.generated.h"

namespace ne RNAMESPACE() {
    class GameInstance;
    class World;
    class Timer;

    /**
     * Describes the order of ticking. Every object of the first tick group will tick first
     * then objects of the second tick group will tick and etc. This allows defining a special
     * order of ticking if some tick functions depend on another or should be executed first/last.
     *
     * Here, "ticking" means calling a function that should be called every frame.
     */
    enum class TickGroup { FIRST, SECOND };

    /**
     * Base class for nodes, allows being spawned in the world, attaching child nodes
     * or being attached to some parent node.
     */
    class RCLASS(Guid("2a721c37-3c22-450c-8dad-7b6985cbbd61")) Node : public Serializable {
        // GameManager will propagate functions to all nodes in the world such as `onBeforeNewFrame`.
        friend class GameManager;

        // World is able to spawn root node.
        friend class World;

    public:
        /**
         * Defines how location, rotation or scale of a node being attached as a child node
         * should change after the attachment process (after `onAfterAttachedToNewParent` was called).
         */
        enum class AttachmentRule {
            RESET_RELATIVE, //< After the new child node was attached, resets its relative location
                            //< or rotation to 0 and relative scale to 1.
            KEEP_RELATIVE,  //< After the new child node was attached, its relative location/rotation/scale
                            //< will stay the same, but world location/rotation/scale might change.
            KEEP_WORLD,     //< After the new child node was attached, its relative
                            //< location/rotation/scale will be recalculated so that its
                            //< world location/rotation/scale will stay the same (as before attached).
        };

        /**
         * Returns the total amount of currently alive (allocated) nodes.
         *
         * @return Number of alive nodes right now.
         */
        static size_t getAliveNodeCount();

        /**
         * Creates a new node with a default name.
         */
        Node();

        /**
         * Creates a new node with the specified name.
         *
         * @param sName Name of this node.
         */
        Node(const std::string& sName);

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
        void setNodeName(const std::string& sName);

        /**
         * Detaches this node from the parent and optionally despawns this node and
         * all of its child nodes if the node was spawned.
         *
         * @remark This function is usually used to mark node (tree) as "to be destroyed", if you
         * just want to change node's parent consider using @ref addChildNode.
         *
         * @remark The node and its child nodes are not guaranteed to be deleted after this
         * function is finished. Deletion is handled automatically by `gc` pointers.
         */
        void detachFromParentAndDespawn();

        /**
         * Attaches a node as a child of this node.
         *
         * @remark If the specified node already has a parent it will change its parent to be
         * a child of this node. This way you can change to which node you are attached.
         *
         * @remark If the specified node needs to be spawned it will queue a deferred task to be added
         * to the World on next frame so input events and @ref onBeforeNewFrame (if enabled) will be called
         * only starting from the next frame.
         *
         * @param pNode        Node to attach as a child. If the specified node is a parent of `this`
         * node the operation will fail and log an error.
         * @param locationRule Only applied if the child node is a SpatialNode, otherwise ignored.
         * Defines how child node's location should change after the attachment process
         * (after `onAfterAttachedToNewParent` was called)
         * @param rotationRule Only applied if the child node is a SpatialNode, otherwise ignored.
         * Defines how child node's rotation should change after the attachment process
         * (after `onAfterAttachedToNewParent` was called)
         * @param scaleRule    Only applied if the child node is a SpatialNode, otherwise ignored.
         * Defines how child node's scale should change after the attachment process
         * (after `onAfterAttachedToNewParent` was called)
         */
        void addChildNode(
            const gc<Node>& pNode,
            AttachmentRule locationRule = AttachmentRule::KEEP_WORLD,
            AttachmentRule rotationRule = AttachmentRule::KEEP_WORLD,
            AttachmentRule scaleRule = AttachmentRule::KEEP_WORLD);

        /**
         * Sets if this node (and node's child nodes) should be serialized as part of a node tree or not.
         *
         * @param bSerialize `true` to serialize, `false` ignore when serializing as part of a node tree.
         */
        void setSerialize(bool bSerialize);

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
        [[nodiscard]] std::optional<Error>
        serializeNodeTree(const std::filesystem::path& pathToFile, bool bEnableBackup);

        /**
         * Returns node's name.
         *
         * @return Node name.
         */
        std::string getNodeName() const;

        /**
         * Returns world's root node.
         *
         * @return `nullptr` if this node is not spawned or was despawned or world is being destroyed (always
         * check returned pointer before doing something), otherwise valid pointer.
         */
        gc<Node> getWorldRootNode();

        /**
         * Returns parent node if this node.
         *
         * @warning Must be used with mutex.
         *
         * @warning Avoid saving returned raw pointer as it points to the node's field and does not guarantee
         * that the node will always live while you hold this pointer. Returning raw pointer in order
         * to avoid creating GC pointers (if you for example only want to check the parent node there's
         * no point in returning a gc pointer), but you can always save returned GC pointer to
         * node's parent if you need.
         *
         * @return `nullptr` as a gc pointer (second value in the pair) if this node has no parent
         * (could only happen when the node is not spawned), otherwise valid gc pointer.
         */
        std::pair<std::recursive_mutex, gc<Node>>* getParentNode();

        /**
         * Returns pointer to child nodes array.
         *
         * @warning Must be used with mutex.
         *
         * @warning Avoid saving returned raw pointer as it points to the node's field and does not guarantee
         * that the node will always live while you hold this pointer. Returning raw pointer in order
         * to avoid creating GC pointers (if you for example only want to iterate over child nodes
         * there's no point in returning gc_vector), but you can always save returned gc_vector
         * or GC pointers to child nodes if you need.
         *
         * @return Array of child nodes.
         */
        std::pair<std::recursive_mutex, gc_vector<Node>>* getChildNodes();

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
         * @return nullptr if not found, otherwise a valid pointer to the node.
         */
        template <typename NodeType>
            requires std::derived_from<NodeType, Node>
        gc<NodeType> getParentNodeOfType(const std::string& sParentNodeName = "");

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
            requires std::derived_from<NodeType, Node>
        gc<NodeType> getChildNodeOfType(const std::string& sChildNodeName = "");

        /**
         * Returns last created game instance.
         *
         * @return Game instance.
         */
        static GameInstance* getGameInstance();

        /**
         * Returns the tick group this node resides in.
         *
         * @return Tick group the node is using.
         */
        TickGroup getTickGroup() const;

        /**
         * Returns a unique ID of the node.
         *
         * @remark Each spawn gives the node a new ID.
         *
         * @return Empty if this node was never spawned, otherwise unique ID of this node.
         */
        std::optional<size_t> getNodeId() const;

        /**
         * Returns whether the @ref onBeforeNewFrame should be called each frame or not.
         *
         * @return Whether the @ref onBeforeNewFrame should be called each frame or not.
         */
        bool isCalledEveryFrame();

        /**
         * Returns whether this node receives input or not.
         *
         * @return Whether this node receives input or not.
         */
        bool isReceivingInput();

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
         * Tells whether or not this node (and node's child nodes) will be serialized as part of a node tree.
         *
         * @return `false` if this node and its child nodes will be ignored when being serialized as part of a
         * node tree, `true` otherwise.
         */
        bool isSerialized() const;

    protected:
        /**
         * Determines if the @ref onBeforeNewFrame should be called each frame or not
         * (disabled by default).
         *
         * @remark Safe to call any time (while spawned/despawned).
         *
         * @param bEnable `true` to enable @ref onBeforeNewFrame, `false` to disable.
         */
        void setIsCalledEveryFrame(bool bEnable);

        /**
         * Sets the tick group in which the node will reside.
         *
         * Tick groups determine the order in which the @ref onBeforeNewFrame functions will be called
         * on nodes. Each frame, @ref onBeforeNewFrame will be called first on the nodes that use
         * the first tick group, then on the nodes that use the second group and etc. This allows
         * defining a special order in which @ref onBeforeNewFrame functions will be called on nodes,
         * thus if you want some nodes to execute their @ref onBeforeNewFrame function only after
         * some other nodes do so, you can define this with tick groups.
         *
         * @remark Tick group is ignored if @ref setIsCalledEveryFrame was not enabled.
         * @remark Typically you should call this function in your node's constructor to determine
         * in which tick group the node will reside.
         * @remark Nodes use the first tick group by default.
         *
         * @warning Calling this function while the node is spawned will cause an error to be shown.
         *
         * @param tickGroup Tick group the node will reside in.
         */
        void setTickGroup(TickGroup tickGroup);

        /**
         * Determines if the input related functions, such as @ref onMouseMove, @ref onMouseScrollMove,
         * @ref onInputActionEvent and @ref onInputAxisEvent will be called or not.
         *
         * @remark Typically you should call this function in your node's constructor to determine
         * if this node should receive input or not.
         * @remark Nodes do not receive input by default.
         * @remark Safe to call any time (while spawned/despawned).
         *
         * @param bEnable Whether the input function should be enabled or not.
         */
        void setIsReceivingInput(bool bEnable);

        /**
         * Creates a new timer and saves it inside of this node to be used while the node is spawned.
         *
         * @warning Do not free (delete) returned pointer.
         * @warning Do not use returned pointer outside of this node object as the timer is only guaranteed
         * to live while the node (that created the timer) is living.
         *
         * @remark Note that although you can create timers while the node is despawned or was not
         * spawned yet any attempt to start a timer while the node is despawned (or not spawned yet)
         * will result in an error being logged.
         * @remark This function exists to add some protection code to not shoot yourself in the foot,
         * such as: Node will automatically stop and disable created timers
         * before @ref onDespawning is called by using Timer::stop(true)
         * so that you don't have to remember to stop created timers. Moreover, if you are using
         * a callback function for the timer's timeout event it's guaranteed that this callback
         * function will only be called while the node is spawned.
         * @remark There is no `removeTimer` function but it may appear in the future
         * (although there's really no point in removing a timer so don't care about it).
         *
         * @param sTimerName Name of this timer (used for logging). Don't add "timer" word to your timer's
         * name as it will be appended in the logs.
         *
         * @return `nullptr` if something went wrong, otherwise a non-owning pointer to the created timer
         * that is guaranteed to be valid while this node object is alive (i.e. even valid when despawned).
         */
        Timer* createTimer(const std::string& sTimerName);

        /**
         * Creates a new notification broadcaster that only accepts callbacks of the specified type.
         *
         * Example:
         * @code
         * // inside of your Node derived class:
         * auto pBroadcaster = createNotificationBroadcaster<void(bool)>();
         * // save broadcaster pointer somewhere
         *
         * // ...
         *
         * // Subscribe.
         * pBroadcaster->subscribe(NodeFunction<void(bool)>(getNodeId().value(), [](bool bParameter){
         *     // callback logic ...
         * });
         *
         * // Notify.
         * pBroadcaster->broadcast(true);
         * @endcode
         *
         * @warning Do not free (delete) returned pointer.
         * @warning Do not use returned pointer outside of this node object as the broadcaster is only
         * guaranteed to live while the node (that created the broadcaster) is living.
         *
         * @remark Note that although you can create broadcasters while the node is despawned or was not
         * spawned yet any attempt to broadcast the notification will be ignored and will do nothing.
         *
         * @return A non-owning pointer to the created broadcaster that is guaranteed to be valid
         * while this node object is alive (i.e. even valid when despawned).
         */
        template <typename FunctionType>
        NodeNotificationBroadcaster<FunctionType>* createNotificationBroadcaster() {
            std::scoped_lock guard(mtxIsSpawned.first, mtxCreatedBroadcasters.first);

            // Create broadcaster.
            auto pNewBroadcaster = std::unique_ptr<NodeNotificationBroadcaster<FunctionType>>(
                new NodeNotificationBroadcaster<FunctionType>());
            const auto pRawBroadcaster = pNewBroadcaster.get();

            // Add to our array.
            mtxCreatedBroadcasters.second.push_back(std::move(pNewBroadcaster));

            if (mtxIsSpawned.second) {
                // Get node ID.
                if (!iNodeId.has_value()) [[unlikely]] {
                    Error error(std::format(
                        "node \"{}\" created a new broadcaster while being spawned but node's ID is empty",
                        sNodeName));
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }

                // Notify broadcaster about node being spawned.
                pRawBroadcaster->onOwnerNodeSpawning(this);
            }

            return pRawBroadcaster;
        }

        /**
         * Returns map of action events that this node is binded to (must be used with mutex).
         * Binded callbacks will be automatically called when an action event is triggered.
         *
         * @remark Input events will be only triggered if the node is spawned.
         * @remark Input events will not be called if @ref setIsReceivingInput was not enabled.
         * @remark Only events in GameInstance's InputManager (GameInstance::getInputManager)
         * will be considered to trigger events in the node.
         *
         * Example:
         * @code
         * const auto iForwardActionId = 0;
         * const auto pActionEvents = getActionEventBindings();
         *
         * std::scoped_lock guard(pActionEvents->first);
         * pActionEvents->second[iForwardActionId] = [&](KeyboardModifiers modifiers, bool bIsPressedDown) {
         *     moveForward(modifiers, bIsPressedDown);
         * };
         * @endcode
         *
         * @return Binded action events.
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<unsigned int, std::function<void(KeyboardModifiers, bool)>>>*
        getActionEventBindings();

        /**
         * Returns map of axis events that this node is binded to (must be used with mutex).
         * Binded callbacks will be automatically called when an axis event is triggered.
         *
         * @remark Input events will be only triggered if the node is spawned.
         * @remark Input events will not be called if @ref setIsReceivingInput was not enabled.
         * @remark Only events in GameInstance's InputManager (GameInstance::getInputManager)
         * will be considered to trigger events in the node.
         *
         * Example:
         * @code
         * const auto iForwardAxisEventId = 0;
         * const auto pAxisEvents = getAxisEventBindings();
         *
         * std::scoped_lock guard(pAxisEvents->first);
         * pAxisEvents->second[iForwardAxisEventId] = [&](KeyboardModifiers modifiers, float input) {
         *     moveForward(modifiers, input);
         * };
         * @endcode
         *
         * @remark Input parameter is a value in range [-1.0f; 1.0f] that describes input.
         *
         * @return Binded action events.
         */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<unsigned int, std::function<void(KeyboardModifiers, float)>>>*
        getAxisEventBindings();

        /**
         * Returns mutex that is generally used to protect/prevent spawning/despawning.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @return Mutex.
         */
        std::recursive_mutex* getSpawnDespawnMutex();

        /**
         * Called when the window received mouse movement.
         *
         * @remark This function will not be called if @ref setIsReceivingInput was not enabled.
         * @remark This function will only be called while this node is spawned.
         *
         * @param xOffset  Mouse X movement delta in pixels (plus if moved to the right,
         * minus if moved to the left).
         * @param yOffset  Mouse Y movement delta in pixels (plus if moved up,
         * minus if moved down).
         */
        virtual void onMouseMove(double xOffset, double yOffset) {}

        /**
         * Called when the window receives mouse scroll movement.
         *
         * @remark This function will not be called if @ref setIsReceivingInput was not enabled.
         * @remark This function will only be called while this node is spawned.
         *
         * @param iOffset Movement offset.
         */
        virtual void onMouseScrollMove(int iOffset) {}

        /**
         * Called before a new frame is rendered.
         *
         * @remark This function is disabled by default, use @ref setIsCalledEveryFrame to enable it.
         * @remark This function will only be called while this node is spawned.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic (if there is any).
         *
         * @param timeSincePrevFrameInSec Also known as deltatime - time in seconds that has passed since
         * the last frame was rendered.
         */
        virtual void onBeforeNewFrame(float timeSincePrevFrameInSec) {}

        /**
         * Called when this node was not spawned previously and it was either attached to a parent node
         * that is spawned or set as world's root node to execute custom spawn logic.
         *
         * @remark This node will be marked as spawned before this function is called.
         *
         * @remark @ref getSpawnDespawnMutex is locked while this function is called.
         *
         * @remark This function is called before any of the child nodes are spawned. If you
         * need to do some logic after child nodes are spawned use @ref onChildNodesSpawned.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onSpawning() {}

        /**
         * Called after @ref onSpawning when this node and all of node's child nodes (at the moment
         * of spawning) were spawned.
         *
         * @remark Generally you might want to prefer to use @ref onSpawning, this function
         * is mostly used to do some logic related to child nodes after all child nodes were spawned
         * (for example if you have a camera child node you can make it active in this function).
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onChildNodesSpawned() {}

        /**
         * Called before this node is despawned from the world to execute custom despawn logic.
         *
         * @remark This node will be marked as despawned after this function is called.
         * @remark This function is called after all child nodes were despawned.
         *
         * @remark @ref getSpawnDespawnMutex is locked while this function is called.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onDespawning() {}

        /**
         * Called before this node or one of the node's parents (in the parent hierarchy)
         * is about to be detached from the current parent node.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         *
         * @remark If this node is being detached from its parent @ref getParentNode will return
         * `nullptr` after this function is finished.
         *
         * @remark This function will also be called on all child nodes after this function
         * is finished.
         *
         * @param bThisNodeBeingDetached `true` if this node is being detached from its parent,
         * `false` if some node in the parent hierarchy is being detached from its parent.
         */
        virtual void onBeforeDetachedFromParent(bool bThisNodeBeingDetached) {}

        /**
         * Called after this node or one of the node's parents (in the parent hierarchy)
         * was attached to a new parent node.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         *
         * @remark This function will also be called on all child nodes after this function
         * is finished.
         *
         * @param bThisNodeBeingAttached `true` if this node was attached to a parent,
         * `false` if some node in the parent hierarchy was attached to a parent.
         */
        virtual void onAfterAttachedToNewParent(bool bThisNodeBeingAttached) {}

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

        /**
         * Enables the specified timer and sets a callback validator or stops and disables the timer.
         *
         * @remark Does nothing if the timer is already in the requested state.
         *
         * @param pTimer   Timer to enable/disable.
         * @param bEnable  New timer state to set.
         *
         * @return `false` if successful, `true` otherwise.
         */
        bool enableTimer(Timer* pTimer, bool bEnable);

        /**
         * Called when a window that owns this game instance receives user
         * input and the input key exists as an action event in the InputManager.
         *
         * @remark This function will not be called if @ref setIsReceivingInput was not enabled.
         * @remark This function will only be called while this node is spawned.
         *
         * @param iActionId      Unique ID of the input action event (from input manager).
         * @param modifiers      Keyboard modifier keys.
         * @param bIsPressedDown Whether the key down event occurred or key up.
         */
        void onInputActionEvent(unsigned int iActionId, KeyboardModifiers modifiers, bool bIsPressedDown);

        /**
         * Called when a window that owns this game instance receives user
         * input and the input key exists as an axis event in the InputManager.
         *
         * @remark This function will not be called if @ref setIsReceivingInput was not enabled.
         * @remark This function will only be called while this node is spawned.
         *
         * @param iAxisEventId  Unique ID of the input axis event (from input manager).
         * @param modifiers     Keyboard modifier keys.
         * @param input         A value in range [-1.0f; 1.0f] that describes input.
         */
        void onInputAxisEvent(unsigned int iAxisEventId, KeyboardModifiers modifiers, float input);

        /** Calls @ref onSpawning on this node and all of its child nodes. */
        void spawn();

        /** Calls @ref onDespawning on this node and all of its child nodes. */
        void despawn();

        /**
         * Calls @ref onAfterAttachedToNewParent on this node and all of its child nodes.
         *
         * @param bThisNodeBeingAttached `true` if this node was attached to a parent,
         * `false` if some node in the parent hierarchy was attached to a parent.
         */
        void notifyAboutAttachedToNewParent(bool bThisNodeBeingAttached);

        /**
         * Calls @ref onBeforeDetachedFromParent on this node and all of its child nodes.
         *
         * @param bThisNodeBeingDetached `true` if this node is being detached from its parent,
         * `false` if some node in the parent hierarchy is being detached from its parent.
         */
        void notifyAboutDetachingFromParent(bool bThisNodeBeingDetached);

        /**
         * Checks if this node has a valid world pointer and if not
         * asks this node's parent and goes up the node hierarchy
         * up to the root node if needed to find valid pointer to world.
         *
         * @return World.
         */
        World* findValidWorld();

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

        /** Node's name. */
        RPROPERTY(Serialize)
        std::string sNodeName;

        /** Attached child nodes. Should be used under the mutex when changing children. */
        std::pair<std::recursive_mutex, gc_vector<Node>> mtxChildNodes;

        /** Attached parent node. */
        std::pair<std::recursive_mutex, gc<Node>> mtxParentNode;

        /** Map of action events that this node is binded to. Must be used with mutex. */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<unsigned int, std::function<void(KeyboardModifiers, bool)>>>
            mtxBindedActionEvents;

        /** Map of axis events that this node is binded to. Must be used with mutex. */
        std::pair<
            std::recursive_mutex,
            std::unordered_map<unsigned int, std::function<void(KeyboardModifiers, float)>>>
            mtxBindedAxisEvents;

        /**
         * Timers creates using @ref createTimer.
         *
         * @warning Don't remove/erase timers from this array because in @ref despawn
         * we might submit a deferred task (while stopping the timer) and will
         * use the timer to check its state in deferred task so we need to make
         * sure that stopped timer will not be deleted while the node exists.
         * Additionally, all users hold raw pointers to timers so they will hit deleted memory
         * in the case of deletion.
         */
        std::pair<std::recursive_mutex, std::vector<std::unique_ptr<Timer>>> mtxCreatedTimers;

        /**
         * Notification broadcasters created using @ref createNotificationBroadcaster.
         *
         * @warning Don't remove/erase broadcasters from this array because it's allowed
         * to use broadcasters while the node is despawned.
         * Additionally, all users hold raw pointers to broadcasters so they will hit deleted memory
         * in the case of deletion.
         */
        std::pair<std::recursive_mutex, std::vector<std::unique_ptr<NodeNotificationBroadcasterBase>>>
            mtxCreatedBroadcasters;

        /** Whether this node is spawned in the world or not. */
        std::pair<std::recursive_mutex, bool> mtxIsSpawned;

        /** Determines if the @ref onBeforeNewFrame should be called each frame or not. */
        std::pair<std::recursive_mutex, bool> mtxIsCalledEveryFrame;

        /**
         * Determines if the input related functions, such as @ref onMouseMove, @ref onMouseScrollMove,
         * @ref onInputActionEvent and @ref onInputAxisEvent will be called or not.
         */
        std::pair<std::recursive_mutex, bool> mtxIsReceivingInput;

        /**
         * Do not delete this pointer. World object that owns this node.
         *
         * @warning Will be initialized after the node is spawned and reset when despawned.
         */
        World* pWorld = nullptr;

        /** Tick group used by this node. */
        TickGroup tickGroup = TickGroup::FIRST;

        /** Unique ID of the spawned node (initialized after the node is spawned). */
        std::optional<size_t> iNodeId;

        /**
         * Defines whether or not this node (and node's child nodes) should be serialized
         * as part of a node tree.
         */
        bool bSerialize = true;

        /** Name of the attribute we use to serialize information about parent node. */
        static inline const auto sParentNodeIdAttributeName = "parent_node_id";

        /** Name of the attribute we use to store a path to an external node tree. */
        static inline const auto sExternalNodeTreePathAttributeName =
            "external_node_tree_path_relative_to_res";

        ne_Node_GENERATED
    };

    template <typename NodeType>
        requires std::derived_from<NodeType, Node>
    gc<NodeType> Node::getParentNodeOfType(const std::string& sParentNodeName) {
        std::scoped_lock guard(mtxParentNode.first);

        // Check if have a parent.
        if (mtxParentNode.second == nullptr) {
            return nullptr;
        }

        // Check parent's type and optionally name.
        if (dynamic_cast<NodeType*>(&*mtxParentNode.second) &&
            (sParentNodeName.empty() || mtxParentNode.second->getNodeName() == sParentNodeName)) {
            return gc_dynamic_pointer_cast<NodeType>(mtxParentNode.second);
        }

        return mtxParentNode.second->getParentNodeOfType<NodeType>(sParentNodeName);
    }

    template <typename NodeType>
        requires std::derived_from<NodeType, Node>
    gc<NodeType> Node::getChildNodeOfType(const std::string& sChildNodeName) {
        std::scoped_lock guard(mtxChildNodes.first);

        for (auto& pChildNode : *mtxChildNodes.second) {
            if (dynamic_cast<NodeType*>(&*pChildNode) &&
                (sChildNodeName.empty() || pChildNode->getNodeName() == sChildNodeName)) {
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
} // namespace ne RNAMESPACE()

File_Node_GENERATED
