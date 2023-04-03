#include "game/nodes/SpatialNode.h"

// Custom.
#include "game/GameInstance.h"
#include "math/MathHelpers.hpp"

#include "SpatialNode.generated_impl.h"

namespace ne {
    SpatialNode::SpatialNode() : SpatialNode("Spatial Node") {}

    SpatialNode::SpatialNode(const std::string& sNodeName) : Node(sNodeName) {
        mtxSpatialParent.second = nullptr;
    }

    void SpatialNode::setRelativeLocation(const glm::vec3& location) {
        std::scoped_lock guard(mtxWorldMatrix.first);

        relativeLocation = location;

        recalculateLocalMatrix();
        recalculateWorldMatrix();
    }

    void SpatialNode::setRelativeRotation(const glm::vec3& rotation) {
        std::scoped_lock guard(mtxWorldMatrix.first);

        relativeRotation = rotation;

        recalculateLocalMatrix();
        recalculateWorldMatrix();
    }

    void SpatialNode::setRelativeScale(const glm::vec3& scale) {
#if defined(DEBUG)
        if (scale.x < 0.0F || scale.y < 0.0F || scale.z < 0.0F) [[unlikely]] {
            Logger::get().warn("avoid using negative scale as it may cause issues", sSpatialNodeLogCategory);
        }
#endif

        std::scoped_lock guard(mtxWorldMatrix.first);

        relativeScale = scale;

        recalculateLocalMatrix();
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

    glm::quat SpatialNode::getWorldRotationQuaternion() {
        std::scoped_lock guard(mtxWorldMatrix.first);
        return mtxWorldMatrix.second.worldRotationQuaternion;
    }

    glm::vec3 SpatialNode::getWorldScale() {
        std::scoped_lock guard(mtxWorldMatrix.first);
        return mtxWorldMatrix.second.worldScale;
    }

    glm::vec3 SpatialNode::getWorldForwardDirection() {
        std::scoped_lock guard(mtxWorldMatrix.first);
        return mtxWorldMatrix.second.worldForward;
    }

    glm::vec3 SpatialNode::getWorldRightDirection() {
        std::scoped_lock guard(mtxWorldMatrix.first);
        return mtxWorldMatrix.second.worldRight;
    }

    void SpatialNode::setWorldLocation(const glm::vec3& location) {
        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "setting world location for node \"{}\" has no effect "
                    "because the node is not spawned in the world",
                    getNodeName()),
                "");
            return;
        }

        std::scoped_lock guard(mtxWorldMatrix.first, mtxSpatialParent.first);

        // See if we have a parent.
        if (mtxSpatialParent.second != nullptr) {
            // Get parent location/rotation/scale.
            const auto parentLocation = mtxSpatialParent.second->getWorldLocation();
            const auto parentRotationQuat = mtxSpatialParent.second->getWorldRotationQuaternion();
            const auto parentScale = mtxSpatialParent.second->getWorldScale();

            // Calculate inverted transformation.
            const auto invertedTranslation = location - parentLocation;
            const auto invertedRotatedTranslation = glm::inverse(parentRotationQuat) * invertedTranslation;
            const auto invertedScale = MathHelpers::calculateReciprocalVector(parentScale);

            // Calculate relative location.
            relativeLocation = invertedRotatedTranslation * invertedScale;
        } else {
            relativeLocation = location;
        }

        recalculateLocalMatrix();
        recalculateWorldMatrix();
    }

    void SpatialNode::setWorldRotation(const glm::vec3& rotation) {
        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "setting world rotation for node \"{}\" has no effect "
                    "because the node is not spawned in the world",
                    getNodeName()),
                "");
            return;
        }

        std::scoped_lock guard(mtxWorldMatrix.first, mtxSpatialParent.first);

        // See if we have a parent.
        if (mtxSpatialParent.second != nullptr) {
            // Don't care for negative scale (mirrors rotations) because it's rarely used
            // and we warn about it.
            const auto inverseParentQuat =
                glm::inverse(mtxSpatialParent.second->getWorldRotationQuaternion());
            const auto rotationQuat = glm::toQuat(MathHelpers::buildRotationMatrix(rotation));

            const glm::vec3 finalRotationRad = glm::eulerAngles(rotationQuat * inverseParentQuat);
            relativeRotation = glm::degrees(finalRotationRad);
        } else {
            relativeRotation = rotation;
        }

        recalculateLocalMatrix();
        recalculateWorldMatrix();
    }

    void SpatialNode::setWorldScale(const glm::vec3& scale) {
        if (!isSpawned()) [[unlikely]] {
            Logger::get().warn(
                fmt::format(
                    "setting world scale for node \"{}\" has no effect "
                    "because the node is not spawned in the world",
                    getNodeName()),
                "");
            return;
        }

#if defined(DEBUG)
        if (scale.x < 0.0F || scale.y < 0.0F || scale.z < 0.0F) [[unlikely]] {
            Logger::get().warn("avoid using negative scale as it may cause issues", sSpatialNodeLogCategory);
        }
#endif

        std::scoped_lock guard(mtxWorldMatrix.first, mtxSpatialParent.first);

        // See if we have a parent.
        if (mtxSpatialParent.second != nullptr) {
            // Get parent scale.
            const auto parentScale = mtxSpatialParent.second->getWorldScale();

            relativeScale = scale * MathHelpers::calculateReciprocalVector(parentScale);
        } else {
            relativeScale = scale;
        }

        recalculateLocalMatrix();
        recalculateWorldMatrix();
    }

    void SpatialNode::onSpawning() {
        Node::onSpawning();

        // No need to notify child nodes since this function is called before any of
        // the child nodes are spawned.
        recalculateWorldMatrix(false);
    }

    glm::mat4x4 SpatialNode::getWorldMatrix() {
        std::scoped_lock guard(mtxWorldMatrix.first);
        return mtxWorldMatrix.second.worldMatrix;
    }

    void SpatialNode::recalculateWorldMatrix(bool bNotifyChildren) {
        // Prepare some variables.
        glm::mat4x4 parentWorldMatrix = glm::identity<glm::mat4x4>();
        glm::vec3 locationRelativeToParentLocalSpace = relativeLocation;

        std::scoped_lock guard(mtxWorldMatrix.first, mtxLocalMatrix.first);

        {
            // See if there is a spatial node in the parent chain.
            std::scoped_lock spatialParentGuard(mtxSpatialParent.first);
            if (mtxSpatialParent.second != nullptr) {
                // Save parent's world matrix.
                parentWorldMatrix = mtxSpatialParent.second->getWorldMatrix();

                // Calculate location relative to parent base axis.
                const auto parentLocalMatrix = mtxSpatialParent.second->getLocalMatrixIncludingParents();

                locationRelativeToParentLocalSpace =
                    parentLocalMatrix * glm::vec4(
                                            locationRelativeToParentLocalSpace,
                                            0.0F); // <- intentionally ignore parent translation

                // Save local matrix including parents.
                mtxLocalMatrix.second.localMatrixIncludingParents =
                    parentLocalMatrix * mtxLocalMatrix.second.localMatrix;
            } else {
                // Save local matrix including parents.
                mtxLocalMatrix.second.localMatrixIncludingParents = mtxLocalMatrix.second.localMatrix;
            }
        }

        // Calculate world matrix without counting the parent.
        const auto myWorldMatrix = glm::translate(locationRelativeToParentLocalSpace) *
                                   MathHelpers::buildRotationMatrix(relativeRotation) *
                                   glm::scale(relativeScale);

        // Recalculate world matrix.
        mtxWorldMatrix.second.worldMatrix = myWorldMatrix * parentWorldMatrix;

        // Decompose world matrix into separate components.
        glm::vec3 scale;
        glm::quat rotation;
        glm::vec3 location;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(mtxWorldMatrix.second.worldMatrix, scale, rotation, location, skew, perspective);

        // Save world location/rotation/scale.
        mtxWorldMatrix.second.worldLocation = location;
        mtxWorldMatrix.second.worldRotation = glm::degrees(glm::eulerAngles(rotation));
        mtxWorldMatrix.second.worldRotationQuaternion = rotation;
        mtxWorldMatrix.second.worldScale = scale;

        // Calculate world forward direction.
        mtxWorldMatrix.second.worldForward =
            glm::normalize(mtxWorldMatrix.second.worldMatrix * glm::vec4(worldForwardDirection, 0.0F));

        // Calculate world right direction.
        mtxWorldMatrix.second.worldRight =
            glm::normalize(mtxWorldMatrix.second.worldMatrix * glm::vec4(worldRightDirection, 0.0F));

        warnIfExceedingWorldBounds();
        onWorldLocationRotationScaleChanged();

        if (bNotifyChildren) {
            // Notify spatial child nodes.
            // (don't unlock our world matrix yet)
            const auto vChildNodes = getChildNodes();
            for (size_t i = 0; i < vChildNodes->size(); i++) {
                recalculateWorldMatrixForNodeAndNotifyChildren(vChildNodes->at(i));
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
            for (size_t i = 0; i < vChildNodes->size(); i++) {
                recalculateWorldMatrixForNodeAndNotifyChildren(vChildNodes->at(i));
            }
        }
    }

    void SpatialNode::onAfterAttachedToNewParent(bool bThisNodeBeingAttached) {
        Node::onAfterAttachedToNewParent(bThisNodeBeingAttached);

        // Find a spatial node in the parent chain and save it.
        std::scoped_lock spatialParentGuard(mtxSpatialParent.first);
        mtxSpatialParent.second = getParentNodeOfType<SpatialNode>();

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

    void SpatialNode::warnIfExceedingWorldBounds() {
        std::scoped_lock guard(mtxSpawning, mtxWorldMatrix.first);
        if (!isSpawned()) {
            return;
        }

        const auto pGameInstance = getGameInstance();
        if (pGameInstance == nullptr) {
            return;
        }

        const float iWorldSize = static_cast<float>(pGameInstance->getWorldSize());

        if (mtxWorldMatrix.second.worldLocation.x >= iWorldSize ||
            mtxWorldMatrix.second.worldLocation.y >= iWorldSize ||
            mtxWorldMatrix.second.worldLocation.z >= iWorldSize) {
            Logger::get().warn(
                fmt::format(
                    "spatial node \"{}\" is exceeding world bounds, node's world location: "
                    "({}, {}, {}), world size: {}",
                    getNodeName(),
                    mtxWorldMatrix.second.worldLocation.x,
                    mtxWorldMatrix.second.worldLocation.y,
                    mtxWorldMatrix.second.worldLocation.z,
                    iWorldSize),
                sSpatialNodeLogCategory);
        }
    }

    void SpatialNode::onAfterDeserialized() {
        Node::onAfterDeserialized();

        // No need to notify children here because:
        // 1. If this is a node tree that is being deserialized, child nodes will be added
        // after this function is finished, once a child node is added it will recalculate its matrix.
        // 2. If this is a single node that is being deserialized, there are no children.
        recalculateWorldMatrix(false);
    }

    void SpatialNode::recalculateLocalMatrix() {
        std::scoped_lock guard(mtxLocalMatrix.first);

        mtxLocalMatrix.second.localMatrix = glm::translate(relativeLocation) *
                                            MathHelpers::buildRotationMatrix(relativeRotation) *
                                            glm::scale(relativeScale);
    }

} // namespace ne
