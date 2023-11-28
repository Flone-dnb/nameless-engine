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

        recalculateAndMarkShaderDataToBeCopiedToGpu();
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
        recalculateAndMarkShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::setLightIntensity(float intensity) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->intensity = std::clamp(intensity, 0.0F, 1.0F);

        // Update shader data.
        recalculateAndMarkShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::setLightDistance(float distance) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->distance = distance;

        // Update shader data.
        recalculateAndMarkShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::setLightInnerConeAngle(float innerConeAngle) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->innerConeAngle = std::clamp(innerConeAngle, 0.0F, maxConeAngle);

        // Make sure outer cone is equal or bigger than inner cone.
        outerConeAngle = std::clamp(outerConeAngle, this->innerConeAngle, maxConeAngle);

        // Update shader data.
        recalculateAndMarkShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::setLightOuterConeAngle(float outerConeAngle) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->outerConeAngle = std::clamp(outerConeAngle, innerConeAngle, maxConeAngle);

        // Mark updated shader data to be later copied to the GPU resource.
        recalculateAndMarkShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::onAfterDeserialized() {
        SpatialNode::onAfterDeserialized();

        // Make sure our intensity is in valid range.
        intensity = std::clamp(intensity, 0.0F, 1.0F);

        // Make sure our cutoff angle is in valid range.
        innerConeAngle = std::clamp(innerConeAngle, 0.0F, maxConeAngle);
        outerConeAngle = std::clamp(outerConeAngle, innerConeAngle, maxConeAngle);

#if defined(DEBUG)
        static_assert(sizeof(SpotlightShaderData) == 80, "consider clamping new parameters here");
#endif
    }

    void* SpotlightNode::onStartedUpdatingShaderData() {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.shaderData;
    }

    void SpotlightNode::onFinishedUpdatingShaderData() { mtxShaderData.first.unlock(); }

    void SpotlightNode::recalculateAndMarkShaderDataToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Copy up to date parameters.
        mtxShaderData.second.shaderData.position = glm::vec4(getWorldLocation(), 1.0F);
        mtxShaderData.second.shaderData.direction = glm::vec4(getWorldForwardDirection(), 0.0F);
        mtxShaderData.second.shaderData.color = glm::vec4(color, 1.0F);
        mtxShaderData.second.shaderData.intensity = intensity;
        mtxShaderData.second.shaderData.distance = distance;
        mtxShaderData.second.shaderData.cosInnerConeAngle = glm::cos(glm::radians(innerConeAngle));
        mtxShaderData.second.shaderData.cosOuterConeAngle = glm::cos(glm::radians(outerConeAngle));

        static_assert(
            maxConeAngle < 80.1F,
            "tan 80+ degrees will increase very fast so keep it away from 90 degrees to avoid huge cone "
            "radius");
        mtxShaderData.second.shaderData.coneBottomRadius =
            glm::tan(glm::radians(outerConeAngle)) * distance *
            1.75F; // TODO: multiply to avoid light culling artifacts at some view angles

#if defined(DEBUG)
        static_assert(sizeof(SpotlightShaderData) == 80, "consider copying new parameters here");
#endif

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

        // Update shader data.
        recalculateAndMarkShaderDataToBeCopiedToGpu();
    }

    float SpotlightNode::getLightInnerConeAngle() const { return innerConeAngle; }

    float SpotlightNode::getLightOuterConeAngle() const { return outerConeAngle; }

}
