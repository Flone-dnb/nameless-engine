#pragma once

// Standard.
#include <memory>
#include <chrono>
#include <string>

// Custom.
#include "game/nodes/Node.h"
#include "misc/GC.hpp"

namespace ne {
    class Game;

    /** Represents arrays of nodes that are marked as "should be called every frame". */
    struct CalledEveryFrameNodes {
        CalledEveryFrameNodes() {
            mtxFirstTickGroup.second = gc_new_vector<Node>();
            mtxSecondTickGroup.second = gc_new_vector<Node>();
        }
        CalledEveryFrameNodes(const CalledEveryFrameNodes&) = delete;
        CalledEveryFrameNodes& operator=(const CalledEveryFrameNodes&) = delete;

        /** Nodes of the first tick group. */
        std::pair<std::recursive_mutex, gc_vector<Node>> mtxFirstTickGroup;
        /** Nodes of the second tick group. */
        std::pair<std::recursive_mutex, gc_vector<Node>> mtxSecondTickGroup;
    };

    /** Owns world's root node. */
    class World {
    public:
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete;
        World& operator=(World&&) = delete;

        /** Destroys the world (if @ref destroyWorld was not called before) and logs about destruction. */
        ~World();

        /**
         * Creates a new world that contains only one node - root node.
         *
         * @param pGame       Game object that owns this world.
         * @param iWorldSize  Size of the world in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.). World size needs to be specified for
         * internal purposes such as Directional Light shadow map size.
         * You don't need to care why we need this information, you only need to know that if
         * you leave world bounds lighting or physics may be incorrect
         * (the editor or engine will warn you if something is leaving world bounds, pay attention
         * to the logs).
         *
         * @return Pointer to the new world instance.
         */
        static std::unique_ptr<World> createWorld(Game* pGame, size_t iWorldSize = 1024);

        /**
         * Loads and deserializes a node tree to be used as a new world.
         *
         * Node tree's root node will be used as world's root node.
         *
         * @param pGame          Game object that owns this world.
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
            Game* pGame, const std::filesystem::path& pathToNodeTree, size_t iWorldSize = 1024);

        /**
         * Clears pointer to the root node which should cause the world
         * to recursively be despawned and destroyed.
         *
         * @remark This function will be called in destructor but you can also call it explicitly.
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
        std::pair<std::recursive_mutex, gc_vector<Node>>* getReceivingInputNodes();

        /**
         * Returns a pointer to world's root node.
         *
         * @return World's root node.
         */
        gc<Node> getRootNode();

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
         * Called from Node to notify the World about a new node being spawned.
         *
         * @param pNode Node that is being spawned.
         */
        void onNodeSpawned(gc<Node> pNode);

        /**
         * Called from Node to notify the World about a node being despawned.
         *
         * @param pNode Node that is being despawned.
         */
        void onNodeDespawned(gc<Node> pNode);

    private:
        /**
         * Creates a new world with the specified root node.
         *
         * @param pGame       Game object that owns this world.
         * @param pRootNode   World's root node.
         * @param iWorldSize  World size in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.).
         */
        World(Game* pGame, gc<Node> pRootNode, size_t iWorldSize);

        /** Do not delete. Owner game object. */
        Game* pGame = nullptr;

        /** Whether the world is destroyed (or being destroyed) and should not be used or not. */
        std::pair<std::recursive_mutex, bool> mtxIsDestroyed;

        /** World's root node. */
        std::pair<std::mutex, gc<Node>> mtxRootNode;

        /** Array of currently spawned nodes that are marked as "should be called every frame". */
        CalledEveryFrameNodes calledEveryFrameNodes;

        /** Array of currently spawned nodes that receive input. */
        std::pair<std::recursive_mutex, gc_vector<Node>> mtxReceivingInputNodes;

        /** World size in game units. */
        size_t iWorldSize = 0;

        /** Time when world was created. */
        std::chrono::steady_clock::time_point timeWhenWorldCreated;

        /** Name of the category used for logging. */
        inline static const char* sWorldLogCategory = "World";
    };
} // namespace ne
