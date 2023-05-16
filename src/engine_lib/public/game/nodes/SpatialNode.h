#pragma once

// Custom.
#include "game/nodes/Node.h"
#include "math/GLMath.hpp"
#include "misc/Globals.h"

#include "SpatialNode.generated.h"

namespace ne RNAMESPACE() {
    /** Represents a node that can have a location, rotation and a scale in a 3D space. */
    class RCLASS(Guid("150d647c-f385-4a11-b585-d059d2be88aa")) SpatialNode : public Node {
        // Calls `applyAttachmentRule`.
        friend class Node;

    public:
        SpatialNode();

        /**
         * Creates a new node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        SpatialNode(const std::string& sNodeName);

        virtual ~SpatialNode() override = default;

        /**
         * Sets node's relative location, if there is another SpatialNode in the parent chain then this
         * location is relative to the first SpatialNode in the parent chain, otherwise if there is no
         * SpatialNode in the parent chain, this location is relative to the world.
         *
         * @param location Relative location.
         */
        void setRelativeLocation(const glm::vec3& location);

        /**
         * Sets node's relative rotation (roll, pitch, yaw in degrees), if there is another SpatialNode in the
         * parent chain then this rotation is relative to the first SpatialNode in the parent chain, otherwise
         * if there is no SpatialNode in the parent chain, this rotation is relative to the world.
         *
         * @param rotation Relative rotation.
         */
        void setRelativeRotation(const glm::vec3& rotation);

        /**
         * Sets node's relative scale, if there is another SpatialNode in the parent chain then this
         * scale is relative to the first SpatialNode in the parent chain, otherwise if there is no
         * SpatialNode in the parent chain, this scale is relative to the world.
         *
         * @param scale Relative scale.
         */
        void setRelativeScale(const glm::vec3& scale);

        /**
         * Sets relative location in the way that the resulting node's location in the world
         * would match the specified location.
         *
         * @remark If the node is not spawned just sets node's relative location.
         *
         * @param location Location that the node should take in the world.
         */
        void setWorldLocation(const glm::vec3& location);

        /**
         * Sets relative rotation in the way that the resulting node's rotation in the world
         * would match the specified rotation.
         *
         * @remark If the node is not spawned just sets node's relative rotation.
         *
         * @param rotation Rotation that the node should take in the world.
         */
        void setWorldRotation(const glm::vec3& rotation);

        /**
         * Sets relative scale in the way that the resulting node's scale in the world
         * would match the specified scale.
         *
         * @remark If the node is not spawned just sets node's relative scale.
         *
         * @param scale Scale that the node should take in the world.
         */
        void setWorldScale(const glm::vec3& scale);

        /**
         * Returns node's relative location (see @ref setRelativeLocation).
         *
         * @return Relative location.
         */
        inline glm::vec3 getRelativeLocation() const { return relativeLocation; }

        /**
         * Returns node's relative rotation in degrees (see @ref setRelativeRotation).
         * Also see @ref getRelativeRotationMatrix.
         *
         * @return Relative rotation.
         */
        inline glm::vec3 getRelativeRotation() const { return relativeRotation; }

        /**
         * Returns node's relative scale (see @ref setRelativeScale).
         *
         * @return Relative scale.
         */
        inline glm::vec3 getRelativeScale() const { return relativeScale; }

        /**
         * Returns a rotation matrix that applies node's relative rotation.
         *
         * @return Rotation matrix.
         */
        glm::mat4x4 getRelativeRotationMatrix();

        /**
         * Returns node's world location (see @ref setWorldLocation).
         *
         * @remark If the node is not spawned and has no parent, returns @ref getRelativeLocation.
         * If the node is not spawned but has a parent, returns its location in the hierarchy.
         *
         * @return Location of the node in the world.
         */
        glm::vec3 getWorldLocation();

        /**
         * Returns node's world rotation in degrees (see @ref setWorldRotation).
         * Also see @ref getWorldRotationQuaternion.
         *
         * @remark If the node is not spawned and has no parent, returns @ref getRelativeRotation.
         * If the node is not spawned but has a parent, returns its rotation in the hierarchy.
         *
         * @return Rotation of the node in the world.
         */
        glm::vec3 getWorldRotation();

        /**
         * Returns node's world rotation in the quaternion form (see @ref getWorldRotation).
         *
         * @return Rotation of the node in the world.
         */
        glm::quat getWorldRotationQuaternion();

        /**
         * Returns node's world scale (see @ref setWorldScale).
         *
         * @remark If the node is not spawned and has no parent, returns @ref getRelativeScale.
         * If the node is not spawned but has a parent, returns its scale in the hierarchy.
         *
         * @return Scale of the node in the world.
         */
        glm::vec3 getWorldScale();

        /**
         * Returns node's forward direction in world space.
         *
         * @return Unit vector that points in the node's world forward direction.
         */
        glm::vec3 getWorldForwardDirection();

        /**
         * Returns node's right direction in world space.
         *
         * @return Unit vector that points in the node's right direction.
         */
        glm::vec3 getWorldRightDirection();

        /**
         * Returns node's up direction in world space.
         *
         * @return Unit vector that points in the node's up direction.
         */
        glm::vec3 getWorldUpDirection();

        /**
         * Returns node's world matrix (matrix that transforms node's data (for example vertices)
         * to world space).
         *
         * @return World matrix.
         */
        glm::mat4x4 getWorldMatrix();

        /**
         * Returns the first (most closer to this node) spatial node in the parent node chain
         * (i.e. cached result of `getParentNodeOfType<SpatialNode>` that can be used without any search
         * operations).
         *
         * @warning Avoid saving returned raw pointer as it points to the node's field and does not guarantee
         * that the node will always live while you hold this pointer. Returning raw pointer in order
         * to avoid creating GC pointers (if you for example only want to check closes parent node),
         * but you can always save GC pointer if you need.
         *
         * @return `nullptr` as a gc pointer (second value in the pair) if there is no SpatialNode in the
         * parent node chain, otherwise closest SpatialNode in the parent node chain.
         */
        std::pair<std::recursive_mutex, gc<SpatialNode>>* getClosestSpatialParent();

    protected:
        /**
         * Called after the object was successfully deserialized.
         * Used to execute post-deserialization logic.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onAfterDeserialized() override;

        /**
         * Called when this node was not spawned previously and it was either attached to a parent node
         * that is spawned or set as world's root node to execute custom spawn logic.
         *
         * @remark This node will be marked as spawned before this function is called.
         * @remark This function is called before any of the child nodes are spawned.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onSpawning() override;

        /**
         * Called after this node or one of the node's parents (in the parent hierarchy)
         * was attached to a new parent node.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         *
         * @remark This function will also be called on all child nodes after this function
         * is finished.
         *
         * @param bThisNodeBeingAttached `true` if this node was attached to a parent,
         * `false` if some node in the parent hierarchy was attached to a parent.
         */
        virtual void onAfterAttachedToNewParent(bool bThisNodeBeingAttached) override;

        /**
         * Called after node's world location/rotation/scale was changed.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         *
         * @remark If you change location/rotation/scale inside of this function,
         * this function will not be called again (no recursion will occur).
         */
        virtual void onWorldLocationRotationScaleChanged(){};

    private:
        /**
         * Called by `Node` class after we have attached to a new parent node and
         * now need to apply attachment rules based on this new parent node.
         *
         * @param locationRule                  Defines how location should change.
         * @param worldLocationBeforeAttachment World location of this node before being attached.
         * @param rotationRule                  Defines how rotation should change.
         * @param worldRotationBeforeAttachment World rotation of this node before being attached.
         * @param scaleRule                     Defines how scale should change.
         * @param worldScaleBeforeAttachment    World scale of this node before being attached.
         */
        void applyAttachmentRule(
            Node::AttachmentRule locationRule,
            const glm::vec3& worldLocationBeforeAttachment,
            Node::AttachmentRule rotationRule,
            const glm::vec3& worldRotationBeforeAttachment,
            Node::AttachmentRule scaleRule,
            const glm::vec3& worldScaleBeforeAttachment);

        /**
         * Recalculates node's world matrix based on the parent world matrix (can be identity
         * if there's node parent) and optionally notifies spatial child nodes.
         *
         * @warning Expects @ref mtxLocalSpace to be up to date (see @ref recalculateLocalMatrix).
         *
         * @param bNotifyChildren Whether to notify spatial child nodes so that could recalculate
         * their world matrix or not.
         */
        void recalculateWorldMatrix(bool bNotifyChildren = true);

        /** Recalculates node's local matrix based on local location/rotation/scale. */
        void recalculateLocalMatrix();

        /**
         * Checks if the specified node is a SpatialNode and calls @ref recalculateWorldMatrix,
         * otherwise calls this function on all child nodes.
         *
         * @param pNode Node to recalculate world matrix for (if SpatialNode).
         */
        void recalculateWorldMatrixForNodeAndNotifyChildren(Node* pNode);

        /** Logs a warning if node's world location exceeds world bounds. */
        void warnIfExceedingWorldBounds();

        /** Small helper struct to keep all world space related information in one place. */
        struct WorldMatrixInformation {
            WorldMatrixInformation() = default;

            /**
             * World location of this node.
             * This value contains the location component of @ref worldMatrix.
             */
            glm::vec3 worldLocation = glm::vec3(0.0F, 0.0F, 0.0F);

            /**
             * World rotation (roll, pitch, yaw in degrees) of this node.
             * This value contains the rotation component of @ref worldMatrix.
             */
            glm::vec3 worldRotation = glm::vec3(0.0F, 0.0F, 0.0F);

            /**
             * World space of this node.
             * This value contains the scale component of @ref worldMatrix.
             */
            glm::vec3 worldScale = glm::vec3(1.0F, 1.0F, 1.0F);

            /** Forward direction of this node in world space. */
            glm::vec3 worldForward = Globals::WorldDirection::forward;

            /** Right direction of this node in world space. */
            glm::vec3 worldRight = Globals::WorldDirection::right;

            /** Up direction of this node in world space. */
            glm::vec3 worldUp = Globals::WorldDirection::up;

            /** Rotation from @ref worldMatrix in the quaternion form. */
            glm::quat worldRotationQuaternion = glm::identity<glm::quat>();

            /**
             * Matrix that combines @ref worldLocation, @ref worldRotation and @ref worldScale.
             * Allows transforming data from node's local space directly into the world space.
             */
            glm::mat4x4 worldMatrix = glm::identity<glm::mat4x4>();

            /** Whether we are in the notification callback or not. */
            bool bInOnWorldLocationRotationScaleChanged = false;
        };

        /** Small helper struct to keep all local space related information in one place. */
        struct LocalSpaceInformation {
            LocalSpaceInformation() = default;

            /** Matrix that describes basis vectors that define node's local space. */
            glm::mat4x4 relativeRotationMatrix = glm::identity<glm::mat4x4>();

            /** Node's relative rotation in quaternion form. */
            glm::quat relativeRotationQuaternion = glm::identity<glm::quat>();
        };

        /**
         * Node's location, if there is another SpatialNode in the parent chain then this location is
         * relative to the first SpatialNode in the parent chain, otherwise if there is no SpatialNode
         * in the parent chain, relative to the world.
         */
        RPROPERTY(Serialize)
        glm::vec3 relativeLocation = glm::vec3(0.0F, 0.0F, 0.0F);

        /**
         * Node's rotation in degrees, if there is another SpatialNode in the parent chain then
         * this rotation is relative to the first SpatialNode in the parent chain, otherwise if there
         * is no SpatialNode in the parent chain, relative to the world.
         */
        RPROPERTY(Serialize)
        glm::vec3 relativeRotation = glm::vec3(0.0F, 0.0F, 0.0F);

        /**
         * Node's scale, if there is another SpatialNode in the parent chain then this scale is
         * relative to the first SpatialNode in the parent chain, otherwise if there is no SpatialNode
         * in the parent chain, relative to the world.
         */
        RPROPERTY(Serialize)
        glm::vec3 relativeScale = glm::vec3(1.0F, 1.0F, 1.0F);

        /** First (most closer to this node) spatial node in the parent chain. */
        std::pair<std::recursive_mutex, gc<SpatialNode>> mtxSpatialParent{};

        /** Matrix that describes basis vectors that define node's local space. */
        std::pair<std::recursive_mutex, LocalSpaceInformation> mtxLocalSpace{};

        /** World related information, must be used with mutex. */
        std::pair<std::recursive_mutex, WorldMatrixInformation> mtxWorldMatrix{};

        /** Name of the category used for logging. */
        static inline const auto sSpatialNodeLogCategory = "Spatial Node";

        ne_SpatialNode_GENERATED
    };
} // namespace ne RNAMESPACE()

File_SpatialNode_GENERATED
