#pragma once

// Custom.
#include "game/nodes/SpatialNode.h"
#include "math/GLMath.hpp"
#include "shader/VulkanAlignmentConstants.hpp"
#include "shader/general/resource/LightingShaderResourceManager.h"
#include "misc/shapes/Cone.h"
#include "render/general/resource/shadow/ShadowMapHandle.h"

#include "SpotlightNode.generated.h"

namespace ne RNAMESPACE() {
    /** Represents a spotlight in world. */
    class RCLASS(Guid("e7b203dc-0f47-43f2-b26d-3b09a5ec1661")) SpotlightNode : public SpatialNode {
        // Renderer reads shadow map handle and index into viewProjection matrix array.
        friend class Renderer;

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
         * Returns the maximum angle for @ref getLightInnerConeAngle and @ref getLightOuterConeAngle.
         *
         * @return Maximum cone angle (in degrees).
         */
        static constexpr float getMaxLightConeAngle() { return maxConeAngle; }

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
         * @param innerConeAngle Angle in degrees in range [0.0; @ref getMaxLightConeAngle] (will be clamped
         * if outside of the range).
         */
        void setLightInnerConeAngle(float innerConeAngle);

        /**
         * Sets angle of spotlight's inner cone (cone that will have hard light edges),
         * see @ref setLightOuterConeAngle for configuring soft light edges.
         *
         * @param outerConeAngle Angle in degrees in range [@ref getLightInnerConeAngle; @ref
         * getMaxLightConeAngle] (will be clamped if outside of the range).
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
         * @return Angle in degrees in range [0.0; @ref getMaxLightConeAngle].
         */
        float getLightInnerConeAngle() const;

        /**
         * Returns light cutoff angle of the outer cone (soft light edge).
         *
         * @return Angle in degrees in range [@ref getLightInnerConeAngle; @ref getMaxLightConeAngle].
         */
        float getLightOuterConeAngle() const;

        /**
         * Returns shape of this light source in world space.
         *
         * @warning Only valid while spawned.
         *
         * @warning Must be used under mutex.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Shape.
         */
        std::pair<std::mutex, Cone>* getShape();

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
            SpotlightShaderData() = default;

            /**
             * Matrix that transforms data (such as positions) to clip (projection) space of the light
             * source (used for shadow mapping).
             */
            alignas(iVkMat4Alignment) glm::mat4 viewProjectionMatrix = glm::identity<glm::mat4>();

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

            /** Radius of cone's bottom part. */
            alignas(iVkScalarAlignment) float coneBottomRadius = 0.0F;

            /** Index in the spot shadow map array where shadow map of this light source is stored. */
            alignas(iVkScalarAlignment) unsigned int iShadowMapIndex = 0;
        };

        /** Groups data related to shaders. */
        struct ShaderData {
            ShaderData() = default;

            /** Groups used in shadow pass. */
            struct ShadowPassDataGroup {
                ShadowPassDataGroup() = default;

                /** Slot to store @ref shaderData. */
                std::unique_ptr<ShaderLightArraySlot> pSlot;

                /** Data to copy to shaders. */
                ShadowPassLightShaderInfo shaderData;
            };

            /** Slot in the array with data of all spawned spotlights. */
            std::unique_ptr<ShaderLightArraySlot> pSpotlightArraySlot;

            /** Groups data used in shadow pass. */
            ShadowPassDataGroup shadowPassData;

            /** Groups data that will be directly copied to the GPU resource. */
            SpotlightShaderData shaderData;
        };

        /**
         * Used by renderer and returns handle to shadow map texture that this light source uses.
         *
         * @remark Do not delete (free) returned pointer.
         *
         * @return `nullptr` if node is not spawned, otherwise valid pointer.
         */
        ShadowMapHandle* getShadowMapHandle() const;

        /**
         * Used by renderer and returns the current index (because it may change later) into the shader array
         * that stores shadow pass info of spawned light sources.
         *
         * @return Index into array.
         */
        unsigned int getIndexIntoShadowPassInfoShaderArray();

        /**
         * Called after the index into a descriptor array of @ref pShadowMapHandle was initialized/changed.
         *
         * @param iNewIndexIntoArray New index to use.
         */
        void onShadowMapArrayIndexChanged(unsigned int iNewIndexIntoArray);

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
         * Callback that will be called by the renderer when it's ready to copy new (updated)
         * `viewProjectionMatrix` of the light source to the GPU resource.
         *
         * @return Pointer to the `viewProjectionMatrix` at @ref mtxShaderData.
         */
        void* onStartedUpdatingShadowPassData();

        /**
         * Called after @ref onStartedUpdatingShadowPassData to notify this node that the renderer has
         * finished copying the data to the GPU resource.
         */
        void onFinishedUpdatingShadowPassData();

        /**
         * (Re)calculates data used for shadow pass and shadow mapping mapping.
         *
         * @remark Does not call @ref markShaderDataToBeCopiedToGpu.
         */
        void recalculateShadowMappingShaderData();

        /**
         * Recalculates shader data according to the current spotlight data and calls @ref
         * markShaderDataToBeCopiedToGpu.
         *
         * @remark Does nothing if the slot is `nullptr`.
         */
        void recalculateAndMarkShaderDataToBeCopiedToGpu();

        /**
         * Marks array slot at @ref mtxShaderData as "needs update" (if the slot is created)
         * to later be copied to the GPU resource.
         *
         * @remark Does nothing if the slot is `nullptr`.
         */
        void markShaderDataToBeCopiedToGpu();

        /**
         * Recalculates @ref mtxShaderData according to the current parameters (state).
         *
         * @remark Takes information from @ref mtxShaderData and expects that it's updated.
         */
        void recalculateShape();

        /** Only valid while spawned. Up to date data that will be copied to the GPU. */
        std::pair<std::recursive_mutex, ShaderData> mtxShaderData;

        /**
         * Stores up-to-date cone shape (in world space) that represents the spotlight.
         *
         * @remark Only valid while spawned.
         */
        std::pair<std::mutex, Cone> mtxShape;

        /**
         * References shadow map of the light source.
         *
         * @remark Only valid while spawned.
         */
        std::unique_ptr<ShadowMapHandle> pShadowMapHandle;

        /** Color of the light source. */
        RPROPERTY(Serialize)
        glm::vec3 color = glm::vec3(1.0F, 1.0F, 1.0F);

        /** Light intensity, valid values range is [0.0F; 1.0F]. */
        RPROPERTY(Serialize)
        float intensity = 1.0F;

        /** Lit distance. */
        RPROPERTY(Serialize)
        float distance = 15.0F; // NOLINT: seems like a pretty good default value

        /**
         * Light cutoff angle (in degrees) of the inner cone (hard light edge).
         * Valid values range is [0.0F, @ref maxConeAngle].
         */
        RPROPERTY(Serialize)
        float innerConeAngle = 25.0F;

        /**
         * Light cutoff angle (in degrees) of the outer cone (soft light edge).
         * Valid values range is [@ref innerConeAngle, @ref maxConeAngle].
         */
        RPROPERTY(Serialize)
        float outerConeAngle = 45.0F;

        /** Maximum value for @ref innerConeAngle and @ref outerConeAngle. */
        static constexpr float maxConeAngle = 80.0F; // NOLINT: max angle that won't cause any visual issues

        ne_SpotlightNode_GENERATED
    };
}

File_SpotlightNode_GENERATED
