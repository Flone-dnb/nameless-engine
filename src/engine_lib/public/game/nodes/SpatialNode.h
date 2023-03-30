#pragma once

// Custom.
#include "game/nodes/Node.h"
#include "math/GLMath.hpp"

#include "SpatialNode.generated.h"

namespace ne RNAMESPACE() {
    /** Represents a node that can have a location, rotation and a scale in a 3D space. */
    class RCLASS(Guid("150d647c-f385-4a11-b585-d059d2be88aa")) SpatialNode : public Node {
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
         * Sets node's relative rotation (in degrees), if there is another SpatialNode in the parent chain
         * then this rotation is relative to the first SpatialNode in the parent chain, otherwise if there is
         * no SpatialNode in the parent chain, this rotation is relative to the world.
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
         * @remark Calling this function while the node is not spawned will have no effect
         * and will produce a warning.
         *
         * @param location Location that the node should take in the world.
         */
        void setWorldLocation(const glm::vec3& location);

        /**
         * Sets relative rotation in the way that the resulting node's rotation in the world
         * would match the specified rotation.
         *
         * @remark Calling this function while the node is not spawned will have no effect
         * and will produce a warning.
         *
         * @param rotation Rotation that the node should take in the world.
         */
        void setWorldRotation(const glm::vec3& rotation);

        /**
         * Sets relative scale in the way that the resulting node's scale in the world
         * would match the specified scale.
         *
         * @remark Calling this function while the node is not spawned will have no effect
         * and will produce a warning.
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
         * Returns node's world location (see @ref setWorldLocation).
         *
         * @return Location of the node in the world.
         */
        glm::vec3 getWorldLocation();

        /**
         * Returns node's world rotation in degrees (see @ref setWorldRotation).
         *
         * @return Rotation of the node in the world.
         */
        glm::vec3 getWorldRotation();

        /**
         * Returns node's world scale (see @ref setWorldScale).
         *
         * @return Scale of the node in the world.
         */
        glm::vec3 getWorldScale();

        /**
         * Returns node's world matrix (matrix that transforms node's data (for example vertices)
         * to world space).
         *
         * @return World matrix.
         */
        glm::mat4x4 getWorldMatrix();

        /**
         * Returns matrix that describes basis vectors that define node's local space.
         *
         * @return Local matrix;
         */
        glm::mat4x4 getLocalMatrix();

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
         */
        virtual void onWorldLocationRotationScaleChanged(){};

    private:
        /**
         * Recalculates node's world matrix based on the parent world matrix (can be identity
         * if there's node parent) and optionally notifies spatial child nodes.
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
        void recalculateWorldMatrixForNodeAndNotifyChildren(gc<Node> pNode);

        /** Logs a warning if node's world location exceeds world bounds. */
        void warnIfExceedingWorldBounds();

        /**
         * Returns @ref getLocalMatrix multiplied by local matrices of parent spatial nodes.
         *
         * @return Local matrix.
         */
        glm::mat4x4 getLocalMatrixIncludingParents();

        /** Small helper struct to keep all world space related information in one place. */
        struct WorldMatrixInformation {
            /**
             * Location in world, includes hierarchy.
             * This value contains the location component of @ref worldMatrix.
             */
            glm::vec3 worldLocation = glm::vec3(0.0F, 0.0F, 0.0F);

            /**
             * Rotation (in degrees) in world, includes hierarchy.
             * This value contains the rotation component of @ref worldMatrix.
             */
            glm::vec3 worldRotation = glm::vec3(0.0F, 0.0F, 0.0F);

            /**
             * Scale in world, includes hierarchy.
             * This value contains the scale component of @ref worldMatrix.
             */
            glm::vec3 worldScale = glm::vec3(1.0F, 1.0F, 1.0F);

            /**
             * Matrix that combines @ref worldLocation, @ref worldRotation and @ref worldScale.
             * Allows transforming data from node's local space directly into the world space.
             */
            glm::mat4x4 worldMatrix = glm::identity<glm::mat4x4>();
        };

        /** Small helper struct to keep all local space related information in one place. */
        struct LocalMatrixInformation {
            /** Matrix that describes basis vectors that define node's local space. */
            glm::mat4x4 localMatrix = glm::identity<glm::mat4x4>();

            /** @ref localMatrix multiplied by local matrices of parent spatial nodes. */
            glm::mat4x4 localMatrixIncludingParents = glm::identity<glm::mat4x4>();
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

        /** Matrix that describes basis vectors that define node's local space. */
        std::pair<std::recursive_mutex, LocalMatrixInformation> mtxLocalMatrix;

        /** World related information, must be used with mutex. */
        std::pair<std::recursive_mutex, WorldMatrixInformation> mtxWorldMatrix;

        /** Name of the category used for logging. */
        static inline const auto sSpatialNodeLogCategory = "Spatial Node";

        ne_SpatialNode_GENERATED
    };
} // namespace ne RNAMESPACE()

File_SpatialNode_GENERATED
