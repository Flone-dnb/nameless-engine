#pragma once

// Custom.
#include "game/nodes/SpatialNode.h"
#include "math/GLMath.hpp"
#include "shader/VulkanAlignmentConstants.hpp"
#include "shader/general/resources/LightingShaderResourceManager.h"

#include "SpotlightNode.generated.h"

namespace ne RNAMESPACE() {
    /** Represents a spotlight in world. */
    class RCLASS(Guid("e7b203dc-0f47-43f2-b26d-3b09a5ec1661")) SpotlightNode : public SpatialNode {
    public:
        SpotlightNode();

        /**
         * Creates a new node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        SpotlightNode(const std::string& sNodeName);

        virtual ~SpotlightNode() override = default;

        /**
         * Sets light's color.
         *
         * @param color Color in RGB format in range [0.0; 1.0].
         */
        void setLightColor(const glm::vec3& color);

        /**
         * Sets light's intensity.
         *
         * @param intensity Intensity in range [0.0; 1.0] (will be clamped if outside of the range).
         */
        void setLightIntensity(float intensity);

        /**
         * Sets lit distance (i.e. attenuation distance).
         *
         * @param distance Lit distance.
         */
        void setLightDistance(float distance);

        /**
         * Sets angle of spotlight's inner cone (cone that will have hard light edges),
         * see @ref setLightOuterConeAngle for configuring soft light edges.
         *
         * @param innerConeAngle Angle in degrees in range [0.0; 180.0] (will be clamped if outside of the
         * range).
         */
        void setLightInnerConeAngle(float innerConeAngle);

        /**
         * Sets angle of spotlight's inner cone (cone that will have hard light edges),
         * see @ref setLightOuterConeAngle for configuring soft light edges.
         *
         * @param outerConeAngle Angle in degrees in range [@ref getLightInnerConeAngle; 180.0]
         * (will be clamped if outside of the range).
         */
        void setLightOuterConeAngle(float outerConeAngle);

        /**
         * Returns color of this light source.
         *
         * @return Color in RGB format in range [0.0; 1.0].
         */
        glm::vec3 getLightColor() const;

        /**
         * Returns intensity of this light source.
         *
         * @return Intensity in range [0.0; 1.0].
         */
        float getLightIntensity() const;

        /**
         * Returns lit distance.
         *
         * @return Distance.
         */
        float getLightDistance() const;

        /**
         * Returns light cutoff angle of the inner cone (hard light edge).
         *
         * @return Angle in degrees in range [0.0; 180.0].
         */
        float getLightInnerConeAngle() const;

        /**
         * Returns light cutoff angle of the outer cone (soft light edge).
         *
         * @return Angle in degrees in range [@ref getLightInnerConeAngle; 180.0].
         */
        float getLightOuterConeAngle() const;

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

        /**
         * Called after node's world location/rotation/scale was changed.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         *
         * @remark If you change location/rotation/scale inside of this function,
         * this function will not be called again (no recursion will occur).
         */
        virtual void onWorldLocationRotationScaleChanged() override;

    private:
        /** Data that will be directly copied into shaders. */
        struct SpotlightShaderData {
            /** Light position in world space. 4th component is not used. */
            alignas(iVkVec4Alignment) glm::vec4 position = glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);

            /** Light forward unit vector (direction). 4th component is not used. */
            alignas(iVkVec4Alignment) glm::vec4 direction = glm::vec4(0.0F, 0.0F, 0.0F, 0.0F);

            /** Light color. 4th component is not used. */
            alignas(iVkVec4Alignment) glm::vec4 color = glm::vec4(1.0F, 1.0F, 1.0F, 1.0F);

            /** Light intensity. */
            alignas(iVkScalarAlignment) float intensity = 1.0F;

            /** Lit distance. */
            alignas(iVkScalarAlignment) float distance = 1.0F;

            /**
             * Cosine of the spotlight's inner cone angle (cutoff).
             *
             * @remark Represents cosine of the cutoff angle on one side from the light direction
             * (not both sides), i.e. this is a cosine of value [0-90] degrees.
             */
            alignas(iVkScalarAlignment) float cosInnerConeAngle = 0.0F;

            /**
             * Cosine of the spotlight's outer cone angle (cutoff).
             *
             * @remark Represents cosine of the cutoff angle on one side from the light direction
             * (not both sides), i.e. this is a cosine of value [0-90] degrees.
             */
            alignas(iVkScalarAlignment) float cosOuterConeAngle = 0.0F;
        };

        /** Groups data related to shaders. */
        struct ShaderData {
            /** Slot in the array with data of all spawned spotlights. */
            std::unique_ptr<ShaderLightArraySlot> pSpotlightArraySlot;

            /** Groups data that will be directly copied to the GPU resource. */
            SpotlightShaderData shaderData;
        };

        /**
         * Callback that will be called by the renderer when it's ready to copy new (updated)
         * @ref mtxShaderData to the GPU resource.
         *
         * @return Pointer to the shader data at @ref mtxShaderData.
         */
        void* onStartedUpdatingShaderData();

        /**
         * Called after @ref onStartedUpdatingShaderData to notify this node that the renderer has finished
         * copying the data to the GPU resource.
         */
        void onFinishedUpdatingShaderData();

        /**
         * Marks array slot at @ref mtxShaderData as "needs update" (if the slot is created)
         * to later be copied to the GPU resource.
         *
         * @remark Does nothing if the slot is `nullptr`.
         */
        void markShaderDataToBeCopiedToGpu();

        /** Only valid while spawned. Up to date data that will be copied to the GPU. */
        std::pair<std::recursive_mutex, ShaderData> mtxShaderData;

        /** Color of the light source. */
        RPROPERTY(Serialize)
        glm::vec3 color = glm::vec3(1.0F, 1.0F, 1.0F);

        /** Light intensity, valid values range is [0.0F; 1.0F]. */
        RPROPERTY(Serialize)
        float intensity = 1.0F;

        /** Lit distance. */
        RPROPERTY(Serialize)
        float distance = 10.0F; // NOLINT: seems like a pretty good default value

        /**
         * Light cutoff angle (in degrees) of the inner cone (hard light edge).
         * Valid values range is [0.0F, 180.0F].
         */
        RPROPERTY(Serialize)
        float innerConeAngle = 90.0F;

        /**
         * Light cutoff angle (in degrees) of the outer cone (soft light edge).
         * Valid values range is [@ref innerConeAngle, 180.0F].
         */
        RPROPERTY(Serialize)
        float outerConeAngle = 120.0F;

        ne_SpotlightNode_GENERATED
    };
}

File_SpotlightNode_GENERATED
