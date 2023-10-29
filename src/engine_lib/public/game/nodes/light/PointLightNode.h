#pragma once

// Custom.
#include "game/nodes/SpatialNode.h"
#include "math/GLMath.hpp"
#include "material/VulkanAlignmentConstants.hpp"
#include "material/resources/LightingShaderResourceManager.h"

#include "PointLightNode.generated.h"

namespace ne RNAMESPACE() {
    /** Represents a point light source in world. */
    class RCLASS(Guid("7890ed17-6efb-43d1-a7ef-aa5a0589921a")) PointLightNode : public SpatialNode {
    public:
        PointLightNode();

        /**
         * Creates a new node with the specified name.
         *
         * @param sNodeName Name of this node.
         */
        PointLightNode(const std::string& sNodeName);

        virtual ~PointLightNode() override = default;

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
         * Sets distance where the light intensity is half the maximum intensity (see @ref setLightIntensity).
         *
         * @param halfDistance Distance where intensity is half the maximum in range is [0.0; +inf]
         * (will be clamped of outside of the range).
         */
        void setLightHalfDistance(float halfDistance);

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
         * Returns distance where the light intensity is half the maximum intensity.
         *
         * @return Distance in range [0.0; +inf].
         */
        float getLightHalfDistance() const;

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
        struct PointLightShaderData {
            /** Light position in world space. 4th component is not used. */
            alignas(iVkVec4Alignment) glm::vec4 position = glm::vec4(0.0F, 0.0F, 0.0F, 1.0F);

            /** Light color. 4th component is not used. */
            alignas(iVkVec4Alignment) glm::vec4 color = glm::vec4(1.0F, 1.0F, 1.0F, 1.0F);

            /** Light intensity. */
            alignas(iVkScalarAlignment) float intensity = 1.0F;

            /** Distance where intensity is half the maximum. */
            alignas(iVkScalarAlignment) float halfDistance = 5.0F; // NOLINT
        };

        /** Groups data related to shaders. */
        struct ShaderData {
            /** Slot in the array with data of all spawned point lights. */
            std::unique_ptr<ShaderLightArraySlot> pPointLightArraySlot;

            /** Groups data that will be directly copied to the GPU resource. */
            PointLightShaderData shaderData;
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

        /**
         * Distance where the light intensity is half the maximal intensity,
         * valid values range is [@ref minimumHalfDistance; +inf].
         */
        RPROPERTY(Serialize)
        float halfDistance = 5.0F; // NOLINT: seems like a pretty good default value

        /** Minimum value for @ref halfDistance to avoid division by zero in shaders. */
        static constexpr float minimumHalfDistance = 0.001F; // NOLINT

        ne_PointLightNode_GENERATED
    };
}

File_PointLightNode_GENERATED
