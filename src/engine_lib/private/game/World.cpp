#include "World.h"

// Custom.
#include "misc/Error.h"
#include "io/Logger.h"

namespace ne {

    World::World(size_t iWorldSize) {
        // Check that world size is power of 2.
        if (!std::has_single_bit(iWorldSize)) {
            Error err(fmt::format(
                "World size {} should be power of 2 (128, 256, 512, 1024, 2048, etc.).", iWorldSize));
            err.showError();
            throw std::runtime_error(err.getError());
        }

        this->iWorldSize = iWorldSize;
        this->pRootNode = std::make_unique<Node>("World Root Node");

        timeWhenWorldCreated = std::chrono::steady_clock::now();

        Logger::get().info("new world created", "");
    }

    World::~World() { Logger::get().info("world's destructor is being called", ""); }

    std::unique_ptr<World> World::createWorld(size_t iWorldSize) {
        return std::unique_ptr<World>(new World(iWorldSize));
    }

    Node* World::getRootNode() const { return pRootNode.get(); }

    float World::getWorldTimeInSeconds() const {
        const auto durationInMs =
            static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - timeWhenWorldCreated)
                                   .count());

        return durationInMs / 1000.0f;
    }

} // namespace ne
