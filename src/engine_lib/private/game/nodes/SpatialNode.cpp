#include "game/nodes/SpatialNode.h"

namespace ne {
    void SpatialNode::setRelativeLocation(const glm::vec3& location) {
        relativeLocation = location;
        recalculateWorldMatrix();
    }

    void SpatialNode::setRelativeRotation(const glm::vec3& rotation) {
        relativeRotation = rotation;
        recalculateWorldMatrix();
    }

    void SpatialNode::setRelativeScale(const glm::vec3& scale) {
        relativeScale = scale;
        recalculateWorldMatrix();
    }

    glm::vec3 SpatialNode::getRelativeLocation() const { return relativeLocation; }

    glm::vec3 SpatialNode::getRelativeRotation() const { return relativeRotation; }

    glm::vec3 SpatialNode::getRelativeScale() const { return relativeScale; }

    glm::vec3 SpatialNode::getWorldLocation() {
        std::scoped_lock guard(mtxWorldMatrix.first);
        return mtxWorldMatrix.second.worldLocation;
    }

    glm::vec3 SpatialNode::getWorldRotation() {
        std::scoped_lock guard(mtxWorldMatrix.first);
        return mtxWorldMatrix.second.worldRotation;
    }

    glm::vec3 SpatialNode::getWorldScale() {
        std::scoped_lock guard(mtxWorldMatrix.first);
        return mtxWorldMatrix.second.worldScale;
    }

    void SpatialNode::setWorldLocation(const glm::vec3& location) {
        std::scoped_lock guard(mtxWorldMatrix.first);
        relativeLocation = location - mtxWorldMatrix.second.worldLocation;
        recalculateWorldMatrix();
    }

    void SpatialNode::setWorldRotation(const glm::vec3& rotation) {
        std::scoped_lock guard(mtxWorldMatrix.first);
        relativeRotation = rotation - mtxWorldMatrix.second.worldRotation;
        recalculateWorldMatrix();
    }

    void SpatialNode::setWorldScale(const glm::vec3& scale) {
        std::scoped_lock guard(mtxWorldMatrix.first);
        relativeScale = scale - mtxWorldMatrix.second.worldScale;
        recalculateWorldMatrix();
    }

    void SpatialNode::onSpawn() {
        Node::onSpawn();

        // No need to notify child nodes since this function is called before any of
        // the child nodes are spawned.
        recalculateWorldMatrix(false);
    }

    glm::mat4x4 SpatialNode::getWorldMatrix() {
        std::scoped_lock guard(mtxWorldMatrix.first);
        return mtxWorldMatrix.second.worldMatrix;
    }

    void SpatialNode::recalculateWorldMatrix(bool bNotifyChildren) {
        glm::mat4x4 parentWorldMatrix = glm::identity<glm::mat4x4>();

        std::scoped_lock guard(mtxWorldMatrix.first);

        // See if there is a spatial node in the parent chain.
        const auto pSpatialParent = getParentNodeOfType<SpatialNode>();
        if (pSpatialParent != nullptr) {
            parentWorldMatrix = pSpatialParent->getWorldMatrix();
        }

        // Calculate world matrix without counting the parent.
        glm::mat4x4 myWorldMatrix =
            glm::scale(relativeScale) *
            glm::rotate(glm::radians(relativeRotation.x), glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::rotate(glm::radians(relativeRotation.y), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::radians(relativeRotation.z), glm::vec3(0.0f, 0.0f, 1.0f)) *
            glm::translate(relativeLocation);

        // Recalculate world matrix.
        mtxWorldMatrix.second.worldMatrix = myWorldMatrix * parentWorldMatrix;

        // Save world matrix components.
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 location;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(mtxWorldMatrix.second.worldMatrix, scale, rotation, location, skew, perspective);
        glm::vec3 rotationRadians = glm::eulerAngles(rotation);
        mtxWorldMatrix.second.worldLocation = location;
        mtxWorldMatrix.second.worldRotation = glm::vec3(
            glm::degrees(rotationRadians.x),
            glm::degrees(rotationRadians.y),
            glm::degrees(rotationRadians.z));
        mtxWorldMatrix.second.worldScale = scale;

        if (bNotifyChildren) {
            // Notify spatial child nodes.
            // (don't unlock our world matrix yet)
            const auto vChildNodes = getChildNodes();
            for (const auto& pChildNode : *vChildNodes) {
                recalculateWorldMatrixForNodeAndNotifyChildren(pChildNode);
            }
        }
    }

    void SpatialNode::recalculateWorldMatrixForNodeAndNotifyChildren(gc<Node> pNode) {
        const auto pSpatialNode = gc_dynamic_pointer_cast<SpatialNode>(pNode);
        if (pSpatialNode != nullptr) {
            pSpatialNode->recalculateWorldMatrix();
        } else {
            // This is not a spatial node, notify children maybe there's a spatial node somewhere.
            const auto vChildNodes = pNode->getChildNodes();
            for (const auto& pChildNode : *vChildNodes) {
                recalculateWorldMatrixForNodeAndNotifyChildren(pChildNode);
            }
        }
    }

    void SpatialNode::onAfterAttachedToNewParent(bool bThisNodeBeingDetached) {
        Node::onAfterAttachedToNewParent(bThisNodeBeingDetached);

        // No need to notify child nodes since this function (on after attached)
        // will be also called on all child nodes.
        recalculateWorldMatrix(false);
    }

} // namespace ne
