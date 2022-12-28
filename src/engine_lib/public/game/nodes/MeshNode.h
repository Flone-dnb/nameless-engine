#pragma once

// Custom.
#include "game/nodes/SpatialNode.h"

#include "MeshNode.generated.h"

namespace ne RNAMESPACE() {
    class Material;

    /** Represents a node that can have 3D geometry to display (mesh). */
    class RCLASS(Guid("d5407ca4-3c2e-4a5a-9ff3-1262b6a4d264")) MeshNode : public SpatialNode {
    public:
        MeshNode();

        /**
         * Creates a new node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        MeshNode(const std::string& sNodeName);

        virtual ~MeshNode() override = default;

        /**
         * Sets material to use.
         *
         * @remark Replaces old (previously used) material.
         *
         * @param pMaterial Material to use.
         */
        void setMaterial(std::shared_ptr<Material> pMaterial);

    protected:
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
        virtual void onSpawn() override;

        /**
         * Called before this node is despawned from the world to execute custom despawn logic.
         *
         * @remark This node will be marked as despawned after this function is called.
         * @remark This function is called after all child nodes were despawned.
         * @remark If node's destructor is called but node is still spawned it will be despawned.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onDespawn() override;

    private:
        /** Used material. Always contains a valid pointer. */
        RPROPERTY(Serialize)
        std::shared_ptr<Material> pMaterial;

        ne_MeshNode_GENERATED
    };
} // namespace )

File_MeshNode_GENERATED
