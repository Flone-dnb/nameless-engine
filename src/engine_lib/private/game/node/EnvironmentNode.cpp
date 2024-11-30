#include "game/node/EnvironmentNode.h"

// Custom.
#include "render/Renderer.h"
#include "game/Window.h"

#include "EnvironmentNode.generated_impl.h"

namespace ne {

    EnvironmentNode::EnvironmentNode() : EnvironmentNode("Environment Node") {}

    EnvironmentNode::EnvironmentNode(const std::string& sNodeName) : Node(sNodeName) {}

    void EnvironmentNode::setAmbientLight(const glm::vec3& ambientLight) {
        this->ambientLight = ambientLight;
    }

    glm::vec3 EnvironmentNode::getAmbientLight() const { return ambientLight; }

    void EnvironmentNode::onSpawning() {
        Node::onSpawning();

        // Get renderer.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();

        // Lock active environment node mutex.
        std::scoped_lock guard(pRenderer->mtxSpawnedEnvironmentNode.first);

        // Make sure there is no another environment node is set in the renderer.
        if (pRenderer->mtxSpawnedEnvironmentNode.second != nullptr) [[unlikely]] {
            // Avoid using pointer of the node from the renderer (maybe it points to a deleted memory).
            Logger::get().warn(std::format(
                "environment node \"{}\" is being spawned but the renderer already "
                "references some another spawned environment node, environment "
                "settings from this node will not be applied as another spawned "
                "environment node is already affecting the environment",
                getNodeName()));
            return;
        }

        // Set this node as the active environment node.
        pRenderer->mtxSpawnedEnvironmentNode.second = this;
    }

    void EnvironmentNode::onDespawning() {
        Node::onDespawning();

        // Get renderer.
        const auto pRenderer = getGameInstance()->getWindow()->getRenderer();

        // Lock active environment node mutex.
        std::scoped_lock guard(pRenderer->mtxSpawnedEnvironmentNode.first);

        // Make sure this node is the active one.
        if (pRenderer->mtxSpawnedEnvironmentNode.second != this) [[unlikely]] {
            // There is another node, do nothing, see `onSpawning` for more details.
            return;
        }

        // Clear active environment node pointer.
        pRenderer->mtxSpawnedEnvironmentNode.second = nullptr;
    }

}
