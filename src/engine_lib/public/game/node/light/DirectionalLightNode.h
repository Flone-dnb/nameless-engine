#pragma once

// Custom.
#include "game/node/SpatialNode.h"
#include "math/GLMath.hpp"
#include "shader/VulkanAlignmentConstants.hpp"
#include "shader/general/resource/LightingShaderResourceManager.h"
#include "render/general/resource/shadow/ShadowMapHandle.h"

#include "DirectionalLightNode.generated.h"

namespace ne RNAMESPACE() {
    /** Represents a directional light source in world. */
    class RCLASS(Guid("7c95023e-c185-46af-8745-79fc0b59bbb3")) DirectionalLightNode : public SpatialNode {
        // Renderer reads shadow map handle and index into viewProjection matrix array.
        friend class Renderer;

    public:
        DirectionalLightNode();

        /**
         * Creates a new node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        DirectionalLightNode(const std::string& sNodeName);

        virtual ~DirectionalLightNode() override = default;

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
        struct DirecionalLightShaderData {
            DirecionalLightShaderData() = default;

            /**
             * Matrix that transforms data (such as positions) to clip (projection) space of the light
             * source (used for shadow mapping).
             */
            alignas(iVkMat4Alignment) glm::mat4 viewProjectionMatrix = glm::identity<glm::mat4>();

            /** Light forward unit vector (direction). 4th component is not used. */
            alignas(iVkVec4Alignment) glm::vec4 direction = glm::vec4(0.0F, 0.0F, 0.0F, 0.0F);

            /** Light color. 4th component is not used. */
            alignas(iVkVec4Alignment) glm::vec4 color = glm::vec4(1.0F, 1.0F, 1.0F, 1.0F);

            /** Light intensity. */
            alignas(iVkScalarAlignment) float intensity = 1.0F;

            /** Index in the directional shadow map array where shadow map of this light source is stored. */
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

            /** Slot in the array with data of all spawned directional lights. */
            std::unique_ptr<ShaderLightArraySlot> pDirectionalLightArraySlot;

            /** Groups data used in shadow pass. */
            ShadowPassDataGroup shadowPassData;

            /** Groups data that will be directly copied to the GPU resource. */
            DirecionalLightShaderData shaderData;
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
         * shadow pass data of the light source to the GPU resource.
         *
         * @return Pointer to the shadow pass data at @ref mtxShaderData.
         */
        void* onStartedUpdatingShadowPassData();

        /**
         * Called after @ref onStartedUpdatingShadowPassData to notify this node that the renderer has
         * finished copying the data to the GPU resource.
         */
        void onFinishedUpdatingShadowPassData();

        /**
         * Marks array slot at @ref mtxShaderData for shadow pass data as "needs
         * update" (if the slot is created) to later be copied to the GPU resource.
         *
         * @remark Does nothing if the slot is `nullptr`.
         */
        void markShadowPassDataToBeCopiedToGpu();

        /**
         * Marks array slot at @ref mtxShaderData as "needs update" (if the slot is created)
         * to later be copied to the GPU resource.
         *
         * @remark Does nothing if the slot is `nullptr`.
         */
        void markShaderDataToBeCopiedToGpu();

        /**
         * Called after the index into a descriptor array of @ref pShadowMapHandle was initialized/changed.
         *
         * @param iNewIndexIntoArray New index to use.
         */
        void onShadowMapArrayIndexChanged(unsigned int iNewIndexIntoArray);

        /**
         * (Re)calculates data used by shaders in shadow pass and shadow mapping.
         *
         * @remark Does not call @ref markShadowPassDataToBeCopiedToGpu or @ref markShaderDataToBeCopiedToGpu.
         */
        void recalculateShadowMappingShaderData();

        /** Only valid while spawned. Up to date data that will be copied to the GPU. */
        std::pair<std::recursive_mutex, ShaderData> mtxShaderData;

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

        ne_DirectionalLightNode_GENERATED
    };
}

File_DirectionalLightNode_GENERATED
