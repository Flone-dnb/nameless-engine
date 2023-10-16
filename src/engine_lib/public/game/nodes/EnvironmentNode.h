#pragma once

// Custom.
#include "game/nodes/Node.h"
#include "math/GLMath.hpp"

#include "EnvironmentNode.generated.h"

namespace ne RNAMESPACE() {
    /** Allows configuring environment settings such as ambient light, skybox, etc. */
    class RCLASS(Guid("69326ac8-9105-446a-8d8a-9e3c12eeccef")) EnvironmentNode : public Node {
    public:
        EnvironmentNode();

        /**
         * Creates a new node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        EnvironmentNode(const std::string& sNodeName);

        virtual ~EnvironmentNode() override = default;

        /**
         * Sets intensity and color of the ambient lighting.
         *
         * @param ambientLight Light color intensity in RGB format in range [0.0; 1.0].
         */
        void setAmbientLight(const glm::vec3& ambientLight);

        /**
         * Returns ambient light color intensity.
         *
         * @return Light color intensity in RGB format in range [0.0; 1.0].
         */
        glm::vec3 getAmbientLight() const;

    protected:
        /**
         * Called when this node was not spawned previously and it was either attached to a parent node
         * that is spawned or set as world's root node to execute custom spawn logic.
         *
         * @remark This node will be marked as spawned before this function is called.
         *
         * @remark This function is called before any of the child nodes are spawned. If you
         * need to do some logic after child nodes are spawned use @ref onChildNodesSpawned.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onSpawning() override;

        /**
         * Called before this node is despawned from the world to execute custom despawn logic.
         *
         * @remark This node will be marked as despawned after this function is called.
         * @remark This function is called after all child nodes were despawned.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onDespawning() override;

    private:
        /** Intensity and color of the ambient lighting. */
        RPROPERTY(Serialize)
        glm::vec3 ambientLight = glm::vec3(0.1F, 0.1F, 0.1F);

        ne_EnvironmentNode_GENERATED
    };
}

File_EnvironmentNode_GENERATED
