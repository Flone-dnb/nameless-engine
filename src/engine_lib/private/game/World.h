#pragma once

// Standard.
#include <memory>
#include <chrono>
#include <string>
#include <variant>
#include <filesystem>
#include <unordered_set>
#include <mutex>
#include <atomic>

// Custom.
#include "misc/Globals.h"
#include "misc/Error.h"

// External.
#include "GcPtr.h"

namespace ne {
    class GameManager;
    class Node;

    /** Represents arrays of nodes that are marked as "should be called every frame". */
    struct CalledEveryFrameNodes {
        CalledEveryFrameNodes() = default;

        CalledEveryFrameNodes(const CalledEveryFrameNodes&) = delete;
        CalledEveryFrameNodes& operator=(const CalledEveryFrameNodes&) = delete;

        /** Nodes of the first tick group. */
        std::pair<std::recursive_mutex, std::unordered_set<Node*>> mtxFirstTickGroup;

        /** Nodes of the second tick group. */
        std::pair<std::recursive_mutex, std::unordered_set<Node*>> mtxSecondTickGroup;
    };

    /**
     * Owns world's root node.
     *
     * @warning @ref destroyWorld must be explicitly called before destroying this object.
     */
    class World {
        // Nodes notify the world about being spawned/despawned.
        friend class Node;

    public:
        World() = delete;

        World(const World&) = delete;
        World& operator=(const World&) = delete;

        World(World&&) = delete;
        World& operator=(World&&) = delete;

        /** Checks that world is destructed correctly (see @ref destroyWorld) and logs in case of error. */
        ~World();

        /**
         * Creates a new world that contains only one node - root node.
         *
         * @param pGameManager GameManager object that owns this world.
         * @param iWorldSize   Size of the world in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.). World size needs to be specified for
         * internal purposes such as Directional Light shadow map size.
         * You don't need to care why we need this information, you only need to know that if
         * you leave world bounds lighting or physics may be incorrect
         * (the editor or engine will warn you if something is leaving world bounds, pay attention
         * to the logs).
         *
         * @return Pointer to the new world instance.
         */
        static std::unique_ptr<World>
        createWorld(GameManager* pGameManager, size_t iWorldSize = Globals::getDefaultWorldSize());

        /**
         * Loads and deserializes a node tree to be used as a new world.
         *
         * Node tree's root node will be used as world's root node.
         *
         * @param pGameManager   GameManager object that owns this world.
         * @param pathToNodeTree Path to the file that contains a node tree to load, the ".toml"
         * extension will be automatically added if not specified.
         * @param iWorldSize     Size of the world in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.). World size needs to be specified for
         * internal purposes such as Directional Light shadow map size.
         * You don't need to care why we need this information, you only need to know that if
         * you leave world bounds lighting or physics may be incorrect
         * (the editor or engine will warn you if something is leaving world bounds, pay attention
         * to the logs).
         *
         * @return Error if failed to deserialize the node tree, otherwise pointer to the new world instance.
         */
        static std::variant<std::unique_ptr<World>, Error> loadNodeTreeAsWorld(
            GameManager* pGameManager,
            const std::filesystem::path& pathToNodeTree,
            size_t iWorldSize = Globals::getDefaultWorldSize());

        /**
         * Returns total amount of currently spawned nodes.
         *
         * @return Total nodes spawned right now.
         */
        size_t getTotalSpawnedNodeCount();

        /**
         * Clears pointer to the root node which should cause the world
         * to recursively be despawned and destroyed.
         *
         * @warning Node despawn process will queue a bunch of deferred tasks that will notify world
         * about nodes being despawned. Make sure to execute all deferred tasks after calling this function
         * and before destroying this object.
         */
        void destroyWorld();

        /**
         * Returns a pointer to array of nodes that should be called every frame (use with mutex).
         *
         * @return Pointer to array of nodes (use with mutex).
         */
        CalledEveryFrameNodes* getCalledEveryFrameNodes();

        /**
         * Returns a pointer to array of nodes that receive input (use with mutex).
         *
         * @return Pointer to array of nodes (use with mutex).
         */
        std::pair<std::recursive_mutex, std::unordered_set<Node*>>* getReceivingInputNodes();

        /**
         * Returns a pointer to world's root node.
         *
         * @return `nullptr` if world is being destroyed, otherwise pointer to world's root node.
         */
        sgc::GcPtr<Node> getRootNode();

        /**
         * Returns the current amount of spawned nodes that are marked as "should be called every frame".
         *
         * @return Amount of spawned nodes that should be called every frame.
         */
        size_t getCalledEveryFrameNodeCount();

        /**
         * Returns time since world creation (in seconds).
         *
         * @return Time since world creation (in seconds).
         */
        float getWorldTimeInSeconds() const;

        /**
         * Returns world size in game units.
         *
         * @return World size.
         */
        size_t getWorldSize() const;

        /**
         * Tells if a node with the specified ID is currently spawned or not.
         *
         * @param iNodeId ID of the node to check.
         *
         * @return `true` if the node is spawned, `false` otherwise.
         */
        bool isNodeSpawned(size_t iNodeId);

    private:
        /**
         * Creates a new world with the specified root node.
         *
         * @param pGameManager GameManager object that owns this world.
         * @param pRootNode    World's root node.
         * @param iWorldSize   World size in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.).
         */
        World(GameManager* pGameManager, sgc::GcPtr<Node> pRootNode, size_t iWorldSize);

        /**
         * Called from Node to notify the World about a new node being spawned.
         *
         * @param pNode Node that is being spawned.
         */
        void onNodeSpawned(Node* pNode);

        /**
         * Called from Node to notify the World about a node being despawned.
         *
         * @param pNode Node that is being despawned.
         */
        void onNodeDespawned(Node* pNode);

        /**
         * Called from Node to notify the World about a spawned node changed its "is called every frame"
         * setting.
         *
         * @warning Should be called AFTER the node has changed its setting and the new state should not
         * be changed while this function is running.
         *
         * @param pNode Node that is changing its setting.
         */
        void onSpawnedNodeChangedIsCalledEveryFrame(Node* pNode);

        /**
         * Called from Node to notify the World about a spawned node changed its "is receiving input"
         * setting.
         *
         * @warning Should be called AFTER the node has changed its setting and the new state should not
         * be changed while this function is running.
         *
         * @param pNode Node that is changing its setting.
         */
        void onSpawnedNodeChangedIsReceivingInput(Node* pNode);

        /**
         * Adds the specified node to the array of "receiving input" nodes
         * (see @ref getReceivingInputNodes).
         *
         * @param pNode Node to add.
         */
        void addNodeToReceivingInputArray(Node* pNode);

        /**
         * Adds the specified node to the arrays of "called every frame" nodes
         * (see @ref calledEveryFrameNodes).
         *
         * @param pNode Node to add.
         */
        void addNodeToCalledEveryFrameArrays(Node* pNode);

        /**
         * Looks if the specified node exists in the arrays of "called every frame" nodes
         * and removes the node from the arrays (see @ref calledEveryFrameNodes).
         *
         * @param pNode Node to remove.
         */
        void removeNodeFromCalledEveryFrameArrays(Node* pNode);

        /**
         * Looks if the specified node exists in the array of "receiving input" nodes
         * and removes the node from the array (see @ref mtxReceivingInputNodes).
         *
         * @param pNode Node to remove.
         */
        void removeNodeFromReceivingInputArray(Node* pNode);

        /** Do not delete. Owner GameManager object. */
        GameManager* pGameManager = nullptr;

        /** Whether the world is destroyed (or being destroyed) and should not be used or not. */
        std::pair<std::recursive_mutex, bool> mtxIsDestroyed;

        /** World's root node. */
        std::pair<std::mutex, sgc::GcPtr<Node>> mtxRootNode;

        /** Array of currently spawned nodes that are marked as "should be called every frame". */
        CalledEveryFrameNodes calledEveryFrameNodes;

        /** Array of currently spawned nodes that receive input. */
        std::pair<std::recursive_mutex, std::unordered_set<Node*>> mtxReceivingInputNodes;

        /** Stores pairs of "Node ID" - "Spawned Node". */
        std::pair<std::recursive_mutex, std::unordered_map<size_t, Node*>> mtxSpawnedNodes;

        /** Total amount of nodes spawned. */
        std::atomic<size_t> iTotalSpawnedNodeCount{0};

        /** World size in game units. */
        const size_t iWorldSize = 0;

        /** Time when world was created. */
        std::chrono::steady_clock::time_point timeWhenWorldCreated;
    };
} // namespace ne
