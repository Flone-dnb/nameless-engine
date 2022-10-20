#include "World.h"

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"
#include "game/Game.h"

namespace ne {

    World::World(Game* pGame, size_t iWorldSize) {
        // Check that world size is power of 2.
        if (!std::has_single_bit(iWorldSize)) {
            Error err(fmt::format(
                "World size {} should be power of 2 (128, 256, 512, 1024, 2048, etc.).", iWorldSize));
            err.showError();
            throw std::runtime_error(err.getError());
        }

        // Check if the game instance exists.
        const auto pGameInstance = pGame->getGameInstance();
        if (!pGameInstance) {
            Error err("An attempt was made to create a new world while GameInstance is not created. "
                      "Are you trying to create a new world in GameInstance's constructor? Use "
                      "`GameInstance::onGameStarted` instead.");
            err.showError();
            throw std::runtime_error(err.getError());
        }

        this->pGame = pGame;
        this->iWorldSize = iWorldSize;
        mtxRootNode.second = gc_new<Node>("World Root Node");
        mtxRootNode.second->pGameInstance = pGameInstance;
        mtxRootNode.second->spawn();

        timeWhenWorldCreated = std::chrono::steady_clock::now();

        Logger::get().info("new world is created", sWorldLogCategory);
    }

    World::~World() { destroyWorld(); }

    std::unique_ptr<World> World::createWorld(Game* pGame, size_t iWorldSize) {
        return std::unique_ptr<World>(new World(pGame, iWorldSize));
    }

    gc<Node> World::getRootNode() {
        std::scoped_lock guard(mtxRootNode.first);
        return mtxRootNode.second;
    }

    float World::getWorldTimeInSeconds() const {
        const auto durationInMs =
            static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - timeWhenWorldCreated)
                                   .count());

        return durationInMs / 1000.0f;
    }

    void World::destroyWorld() {
        std::scoped_lock guard(mtxRootNode.first);

        if (!mtxRootNode.second)
            return;

        Logger::get().info("world is being destroyed, destroying the root node...", sWorldLogCategory);

        // Mark root node as not used explicitly.
        mtxRootNode.second->despawn(); // explicitly despawn root to despawn all nodes.
        mtxRootNode.second = nullptr;

        // Run GC.
        pGame->queueGarbageCollection({});
    }

} // namespace ne
