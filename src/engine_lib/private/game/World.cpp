#include "World.h"

// Custom.
#include "misc/Error.h"

namespace ne {

    World::World(size_t iWorldSize, std::unique_ptr<Node> pRootNode) {
        if (!std::has_single_bit(iWorldSize)) {
            Error err(fmt::format(
                "World size {} should be power of 2 (128, 256, 512, 1024, 2048, etc.).", iWorldSize));
            err.showError();
            throw std::runtime_error(err.getError());
        }

        this->iWorldSize = iWorldSize;
        this->pRootNode = std::move(pRootNode);

        timeWhenWorldCreated = std::chrono::steady_clock::now();
    }

    Node* World::getRootNode() const { return pRootNode.get(); }

} // namespace ne
