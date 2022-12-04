#include "game/nodes/SpatialNode.h"

namespace ne {
    SpatialNode::SpatialNode() : SpatialNode("Spatial Node") {}

    SpatialNode::SpatialNode(const std::string& sNodeName) : Node(sNodeName) {}

    void SpatialNode::setRelativeLocation(const glm::vec3& location) {
        relativeLocation = location;
        recalculateWorldMatrix();
    }

    void SpatialNode::setRelativeRotation(const glm::vec3& rotation) {
        relativeRotation = rotation;

        {
            // Recalculate basis vectors.
            std::scoped_lock guard(mtxLocalMatrix.first);

            mtxLocalMatrix.second.localMatrix =
                glm::rotate(glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f)) *
                glm::rotate(glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::rotate(glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
            mtxLocalMatrix.second.localMatrixIncludingParents = mtxLocalMatrix.second.localMatrix;
        }

        recalculateWorldMatrix();
    }

    void SpatialNode::setRelativeScale(const glm::vec3& scale) {
        relativeScale = scale;
        recalculateWorldMatrix();
    }

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
        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "setting world location for node \"{}\" has no effect "
                    "because the node is not spawned in the world",
                    getName()),
                "");
            return;
        }

        std::scoped_lock guard(mtxWorldMatrix.first);
        relativeLocation = location - mtxWorldMatrix.second.worldLocation;
        recalculateWorldMatrix();
    }

    void SpatialNode::setWorldRotation(const glm::vec3& rotation) {
        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "setting world rotation for node \"{}\" has no effect "
                    "because the node is not spawned in the world",
                    getName()),
                "");
            return;
        }

        std::scoped_lock guard(mtxWorldMatrix.first);
        relativeRotation = rotation - mtxWorldMatrix.second.worldRotation;
        recalculateWorldMatrix();
    }

    void SpatialNode::setWorldScale(const glm::vec3& scale) {
        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "setting world scale for node \"{}\" has no effect "
                    "because the node is not spawned in the world",
                    getName()),
                "");
            return;
        }

        std::scoped_lock guard(mtxWorldMatrix.first);
        relativeScale = scale / mtxWorldMatrix.second.worldScale;
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
        glm::vec3 locationRelativeToParentLocalSpace = relativeLocation;

        std::scoped_lock guard(mtxWorldMatrix.first, mtxLocalMatrix.first);

        // See if there is a spatial node in the parent chain.
        const auto pSpatialParent = getParentNodeOfType<SpatialNode>();
        if (pSpatialParent != nullptr) {
            parentWorldMatrix = pSpatialParent->getWorldMatrix();

            const auto parentLocalMatrixIncludingParents = pSpatialParent->getLocalMatrixIncludingParents();
            locationRelativeToParentLocalSpace =
                glm::vec4(locationRelativeToParentLocalSpace, 0.0f) * parentLocalMatrixIncludingParents;

            mtxLocalMatrix.second.localMatrixIncludingParents =
                mtxLocalMatrix.second.localMatrix * parentLocalMatrixIncludingParents;
        }

        // Calculate world matrix without counting the parent.
        const auto myWorldMatrix =
            glm::translate(locationRelativeToParentLocalSpace) *
            glm::rotate(glm::radians(relativeRotation.z), glm::vec3(0.0f, 0.0f, 1.0f)) *
            glm::rotate(glm::radians(relativeRotation.y), glm::vec3(0.0f, 1.0f, 0.0f)) *
            glm::rotate(glm::radians(relativeRotation.x), glm::vec3(1.0f, 0.0f, 0.0f)) *
            glm::scale(relativeScale);

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

    void SpatialNode::onAfterAttachedToNewParent(bool bThisNodeBeingAttached) {
        Node::onAfterAttachedToNewParent(bThisNodeBeingAttached);

        // No need to notify child nodes since this function (on after attached)
        // will be also called on all child nodes.
        recalculateWorldMatrix(false);
    }

    glm::mat4x4 SpatialNode::getLocalMatrix() {
        std::scoped_lock guard(mtxLocalMatrix.first);
        return mtxLocalMatrix.second.localMatrix;
    }

    glm::mat4x4 SpatialNode::getLocalMatrixIncludingParents() {
        std::scoped_lock guard(mtxLocalMatrix.first);
        return mtxLocalMatrix.second.localMatrixIncludingParents;
    }

} // namespace ne
