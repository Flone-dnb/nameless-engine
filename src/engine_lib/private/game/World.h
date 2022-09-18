#pragma once

// STL.
#include <memory>
#include <chrono>
#include <string>

// Custom.
#include "game/nodes/Node.h"

namespace ne {
    /** Owns world's root node. */
    class World {
    public:
        World(const World&) = delete;
        World& operator=(const World&) = delete;
        World(World&&) = delete;
        World& operator=(World&&) = delete;

        /** Logs destruction. */
        ~World();

        /**
         * Creates a new world with a root node to store world's node tree.
         *
         * @param iWorldSize    Size of the world in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.). World size needs to be specified for
         * internal purposes such as Directional Light shadow map size, maybe for the size
         * of correct physics calculations and etc. You don't need to care why we need this
         * information, you only need to know that if you leave world bounds lighting
         * or physics may be incorrect (the editor and logs should help you identify this case).
         *
         * @return New world instance.
         */
        static std::unique_ptr<World> createWorld(size_t iWorldSize = 1024);

        /**
         * Returns a non owning pointer to world's root node.
         *
         * @return Do not delete returned pointer. World's root node.
         */
        Node* getRootNode() const;

        /**
         * Returns time since world creation (in seconds).
         *
         * @return Time since world creation (in seconds).
         */
        float getWorldTimeInSeconds() const;

    private:
        /**
         * Creates a new world with the specified root node.
         *
         * @param iWorldSize World size in game units. Must be power of 2
         * (128, 256, 512, 1024, 2048, etc.).
         */
        World(size_t iWorldSize);

        /** World's root node. */
        std::unique_ptr<Node> pRootNode;

        /** World size in game units. */
        size_t iWorldSize = 0;

        /** Time when world was created. */
        std::chrono::steady_clock::time_point timeWhenWorldCreated;
    };
} // namespace ne
