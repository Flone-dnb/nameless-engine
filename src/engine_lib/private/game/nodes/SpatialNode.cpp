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

        relativeRotation.x = MathHelpers::normalizeValue(rotation.x, -360.0F, 360.0F); // NOLINT
        relativeRotation.y = MathHelpers::normalizeValue(rotation.y, -360.0F, 360.0F); // NOLINT
        relativeRotation.z = MathHelpers::normalizeValue(rotation.z, -360.0F, 360.0F); // NOLINT

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

    glm::vec3 SpatialNode::getWorldUpDirection() {
        std::scoped_lock guard(mtxWorldMatrix.first);
        return mtxWorldMatrix.second.worldUp;
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

        glm::vec3 targetWorldRotation;
        targetWorldRotation.x = MathHelpers::normalizeValue(rotation.x, -360.0F, 360.0F); // NOLINT
        targetWorldRotation.y = MathHelpers::normalizeValue(rotation.y, -360.0F, 360.0F); // NOLINT
        targetWorldRotation.z = MathHelpers::normalizeValue(rotation.z, -360.0F, 360.0F); // NOLINT

        std::scoped_lock guard(mtxWorldMatrix.first, mtxSpatialParent.first);

        // See if we have a parent.
        if (mtxSpatialParent.second != nullptr) {
            // Don't care for negative scale (mirrors rotations) because it's rarely used
            // and we warn about it.
            const auto inverseParentQuat =
                glm::inverse(mtxSpatialParent.second->getWorldRotationQuaternion());
            const auto rotationQuat = glm::toQuat(MathHelpers::buildRotationMatrix(targetWorldRotation));

            relativeRotation = glm::degrees(glm::eulerAngles(inverseParentQuat * rotationQuat));
        } else {
            relativeRotation = targetWorldRotation;
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
        glm::quat parentWorldRotationQuat = glm::identity<glm::quat>();
        glm::vec3 parentWorldScale = glm::vec3(1.0F, 1.0F, 1.0F);

        std::scoped_lock guard(mtxWorldMatrix.first, mtxLocalSpace.first);

        {
            // See if there is a spatial node in the parent chain.
            std::scoped_lock spatialParentGuard(mtxSpatialParent.first);
            if (mtxSpatialParent.second != nullptr) {
                // Save parent's world information.
                parentWorldMatrix = mtxSpatialParent.second->getWorldMatrix();
                parentWorldRotationQuat = mtxSpatialParent.second->getWorldRotationQuaternion();
                parentWorldScale = mtxSpatialParent.second->getWorldScale();
            }
        }

        // Calculate world matrix without counting the parent.
        const auto myWorldMatrix = glm::translate(relativeLocation) *
                                   mtxLocalSpace.second.relativeRotationMatrix * glm::scale(relativeScale);

        // Recalculate world matrix.
        mtxWorldMatrix.second.worldMatrix = parentWorldMatrix * myWorldMatrix;

        // Save world location.
        mtxWorldMatrix.second.worldLocation =
            parentWorldMatrix *
            glm::vec4(relativeLocation, 1.0F); // don't apply relative rotation/scale to world location

        // Save world rotation.
        mtxWorldMatrix.second.worldRotationQuaternion =
            parentWorldRotationQuat * mtxLocalSpace.second.relativeRotationQuaternion;
        mtxWorldMatrix.second.worldRotation =
            glm::degrees(glm::eulerAngles(mtxWorldMatrix.second.worldRotationQuaternion));

        // Save world scale.
        mtxWorldMatrix.second.worldScale = parentWorldScale * relativeScale;

        // Calculate world forward direction.
        mtxWorldMatrix.second.worldForward =
            glm::normalize(mtxWorldMatrix.second.worldMatrix * glm::vec4(worldForwardDirection, 0.0F));

        // Calculate world right direction.
        mtxWorldMatrix.second.worldRight =
            glm::normalize(mtxWorldMatrix.second.worldMatrix * glm::vec4(worldRightDirection, 0.0F));

        // Calculate world up direction.
        mtxWorldMatrix.second.worldUp =
            glm::cross(mtxWorldMatrix.second.worldForward, mtxWorldMatrix.second.worldRight);

        warnIfExceedingWorldBounds();

        if (mtxWorldMatrix.second.bInOnWorldLocationRotationScaleChanged) {
            // We came here from a `onWorldLocationRotationScaleChanged` call, stop recursion and
            // don't notify children as it will be done when this `onWorldLocationRotationScaleChanged` call
            // will be finished.
            return;
        }

        mtxWorldMatrix.second.bInOnWorldLocationRotationScaleChanged = true;
        onWorldLocationRotationScaleChanged();
        mtxWorldMatrix.second.bInOnWorldLocationRotationScaleChanged = false;

        if (bNotifyChildren) {
            // Notify spatial child nodes.
            // (don't unlock our world matrix yet)
            const auto pMtxChildNodes = getChildNodes();
            std::scoped_lock childNodesGuard(pMtxChildNodes->first);
            for (const auto& pNode : *pMtxChildNodes->second) {
                recalculateWorldMatrixForNodeAndNotifyChildren(&*pNode);
            }
        }
    }

    void SpatialNode::recalculateWorldMatrixForNodeAndNotifyChildren(Node* pNode) {
        const auto pSpatialNode = dynamic_cast<SpatialNode*>(pNode);
        if (pSpatialNode != nullptr) {
            pSpatialNode->recalculateWorldMatrix();
        } else {
            // This is not a spatial node, notify children maybe there's a spatial node somewhere.
            const auto pMtxChildNodes = pNode->getChildNodes();
            std::scoped_lock childNodesGuard(pMtxChildNodes->first);
            for (const auto& pNode : *pMtxChildNodes->second) {
                recalculateWorldMatrixForNodeAndNotifyChildren(&*pNode);
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

        recalculateLocalMatrix();

        // No need to notify children here because:
        // 1. If this is a node tree that is being deserialized, child nodes will be added
        // after this function is finished, once a child node is added it will recalculate its matrix.
        // 2. If this is a single node that is being deserialized, there are no children.
        recalculateWorldMatrix(false);
    }

    void SpatialNode::recalculateLocalMatrix() {
        std::scoped_lock guard(mtxLocalSpace.first);

        mtxLocalSpace.second.relativeRotationMatrix = MathHelpers::buildRotationMatrix(relativeRotation);
        mtxLocalSpace.second.relativeRotationQuaternion =
            glm::toQuat(mtxLocalSpace.second.relativeRotationMatrix);
    }

    glm::mat4x4 SpatialNode::getRelativeRotationMatrix() {
        std::scoped_lock guard(mtxLocalSpace.first);
        return mtxLocalSpace.second.relativeRotationMatrix;
    }

    std::pair<std::recursive_mutex, gc<SpatialNode>>* SpatialNode::getClosestSpatialParent() {
        return &mtxSpatialParent;
    }

} // namespace ne
