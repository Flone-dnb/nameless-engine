#include "game/nodes/light/SpotlightNode.h"

// Custom.
#include "game/Window.h"
#include "render/Renderer.h"

#include "SpotlightNode.generated_impl.h"

namespace ne {

    SpotlightNode::SpotlightNode() : SpotlightNode("Spotlight Node") {}

    SpotlightNode::SpotlightNode(const std::string& sNodeName) : SpatialNode(sNodeName) {}

    void SpotlightNode::onSpawning() {
        SpatialNode::onSpawning();

        // Prepare shader data.
        std::scoped_lock guard(mtxShaderData.first);

        // Copy up to date parameters.
        mtxShaderData.second.shaderData.position = glm::vec4(getWorldLocation(), 1.0F);
        mtxShaderData.second.shaderData.direction = glm::vec4(getWorldForwardDirection(), 0.0F);
        mtxShaderData.second.shaderData.color = glm::vec4(color, 1.0F);
        mtxShaderData.second.shaderData.intensity = intensity;
        mtxShaderData.second.shaderData.distance = distance;
        mtxShaderData.second.shaderData.cosInnerConeAngle =
            glm::cos(glm::radians(innerConeAngle / 2.0F)); // NOLINT
        mtxShaderData.second.shaderData.cosOuterConeAngle =
            glm::cos(glm::radians(outerConeAngle / 2.0F)); // NOLINT
        mtxShaderData.second.shaderData.coneBottomRadius = glm::tan(glm::radians(outerConeAngle)) * distance;
#if defined(DEBUG)
        static_assert(sizeof(SpotlightShaderData) == 80, "consider copying new parameters here");
#endif

        // Reserve a slot in the spotlight shader data array
        // so that our parameters will be available in the shaders.
        const auto pSpotlightDataArray = getGameInstance()
                                             ->getWindow()
                                             ->getRenderer()
                                             ->getLightingShaderResourceManager()
                                             ->getSpotlightDataArray();
        auto result = pSpotlightDataArray->reserveNewSlot(
            sizeof(SpotlightShaderData),
            [this]() { return onStartedUpdatingShaderData(); },
            [this]() { onFinishedUpdatingShaderData(); });
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Save received slot.
        mtxShaderData.second.pSpotlightArraySlot =
            std::get<std::unique_ptr<ShaderLightArraySlot>>(std::move(result));
    }

    void SpotlightNode::onDespawning() {
        SpatialNode::onDespawning();

        // Mark slot as unused.
        std::scoped_lock guard(mtxShaderData.first);
        mtxShaderData.second.pSpotlightArraySlot = nullptr;
    }

    void SpotlightNode::setLightColor(const glm::vec3& color) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->color = color;

        // Update shader data.
        mtxShaderData.second.shaderData.color = glm::vec4(this->color, 1.0F);

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::setLightIntensity(float intensity) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->intensity = std::clamp(intensity, 0.0F, 1.0F);

        // Update shader data.
        mtxShaderData.second.shaderData.intensity = this->intensity;

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::setLightDistance(float distance) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->distance = distance;

        // Update shader data.
        mtxShaderData.second.shaderData.distance = this->distance;

        mtxShaderData.second.shaderData.coneBottomRadius =
            glm::tan(glm::radians(this->outerConeAngle)) * this->distance;

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::setLightInnerConeAngle(float innerConeAngle) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->innerConeAngle = std::clamp(innerConeAngle, 0.0F, 180.0F); // NOLINT

        // Update shader data.
        mtxShaderData.second.shaderData.cosInnerConeAngle =
            glm::cos(glm::radians(this->innerConeAngle / 2.0F)); // NOLINT

        // Make sure outer cone is equal or bigger than inner cone.
        if (innerConeAngle > outerConeAngle) {
            // Move outer angle to match the inner angle.
            outerConeAngle = innerConeAngle;

            // Update shader data.
            mtxShaderData.second.shaderData.cosOuterConeAngle =
                mtxShaderData.second.shaderData.cosInnerConeAngle;

            mtxShaderData.second.shaderData.coneBottomRadius =
                glm::tan(glm::radians(outerConeAngle)) * distance;
        }

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::setLightOuterConeAngle(float outerConeAngle) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->outerConeAngle = std::clamp(outerConeAngle, innerConeAngle, 180.0F); // NOLINT

        // Update shader data.
        mtxShaderData.second.shaderData.cosOuterConeAngle =
            glm::cos(glm::radians(this->outerConeAngle / 2.0F)); // NOLINT

        mtxShaderData.second.shaderData.coneBottomRadius =
            glm::tan(glm::radians(this->outerConeAngle)) * distance;

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::onAfterDeserialized() {
        SpatialNode::onAfterDeserialized();

        // Make sure our intensity is in range [0.0; 1.0].
        intensity = std::clamp(intensity, 0.0F, 1.0F);

        // Make sure our cutoff angle is in range [0.0; 180.0].
        innerConeAngle = std::clamp(innerConeAngle, 0.0F, 180.0F);           // NOLINT
        outerConeAngle = std::clamp(outerConeAngle, innerConeAngle, 180.0F); // NOLINT

#if defined(DEBUG)
        static_assert(sizeof(SpotlightShaderData) == 80, "consider clamping new parameters here");
#endif
    }

    void* SpotlightNode::onStartedUpdatingShaderData() {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.shaderData;
    }

    void SpotlightNode::onFinishedUpdatingShaderData() { mtxShaderData.first.unlock(); }

    void SpotlightNode::markShaderDataToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Make sure the slot exists.
        if (mtxShaderData.second.pSpotlightArraySlot == nullptr) {
            return;
        }

        // Mark as "needs update".
        mtxShaderData.second.pSpotlightArraySlot->markAsNeedsUpdate();
    }

    glm::vec3 SpotlightNode::getLightColor() const { return color; }

    float SpotlightNode::getLightIntensity() const { return intensity; }

    float SpotlightNode::getLightDistance() const { return distance; }

    void SpotlightNode::onWorldLocationRotationScaleChanged() {
        SpatialNode::onWorldLocationRotationScaleChanged();

        std::scoped_lock guard(mtxShaderData.first);

        // Update shader data.
        mtxShaderData.second.shaderData.position = glm::vec4(getWorldLocation(), 1.0F);
        mtxShaderData.second.shaderData.direction = glm::vec4(getWorldForwardDirection(), 0.0F);

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    float SpotlightNode::getLightInnerConeAngle() const { return innerConeAngle; }

    float SpotlightNode::getLightOuterConeAngle() const { return outerConeAngle; }

}
