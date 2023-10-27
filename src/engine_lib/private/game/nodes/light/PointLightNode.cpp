#include "game/nodes/light/PointLightNode.h"

// Custom.
#include "game/Window.h"
#include "render/Renderer.h"

#include "PointLightNode.generated_impl.h"

namespace ne {

    PointLightNode::PointLightNode() : PointLightNode("Point Light Node") {}

    PointLightNode::PointLightNode(const std::string& sNodeName) : SpatialNode(sNodeName) {}

    void PointLightNode::onSpawning() {
        SpatialNode::onSpawning();

        // Prepare shader data.
        std::scoped_lock guard(mtxShaderData.first);

        // Copy up to date parameters.
        mtxShaderData.second.shaderData.position = glm::vec4(getWorldLocation(), 1.0F);
        mtxShaderData.second.shaderData.color = glm::vec4(color, 1.0F);
        mtxShaderData.second.shaderData.intensity = intensity;
        mtxShaderData.second.shaderData.halfDistance = halfDistance;
#if defined(DEBUG)
        static_assert(sizeof(PointLightShaderData) == 48, "consider copying new parameters here");
#endif

        // Reserve a slot in the point light shader data array
        // so that our parameters will be available in the shaders.
        const auto pPointLightDataArray = getGameInstance()
                                              ->getWindow()
                                              ->getRenderer()
                                              ->getLightingShaderResourceManager()
                                              ->getPointLightDataArray();
        auto result = pPointLightDataArray->reserveNewSlot(
            sizeof(PointLightShaderData),
            [this]() { return onStartedUpdatingShaderData(); },
            [this]() { onFinishedUpdatingShaderData(); });
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Save received slot.
        mtxShaderData.second.pPointLightArraySlot =
            std::get<std::unique_ptr<ShaderLightArraySlot>>(std::move(result));
    }

    void PointLightNode::onDespawning() {
        SpatialNode::onDespawning();

        // Mark slot as unused.
        std::scoped_lock guard(mtxShaderData.first);
        mtxShaderData.second.pPointLightArraySlot = nullptr;
    }

    void PointLightNode::setLightColor(const glm::vec3& color) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->color = color;

        // Update shader data.
        mtxShaderData.second.shaderData.color = glm::vec4(this->color, 1.0F);

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void PointLightNode::setLightIntensity(float intensity) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->intensity = std::clamp(intensity, 0.0F, 1.0F);

        // Update shader data.
        mtxShaderData.second.shaderData.intensity = this->intensity;

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void PointLightNode::setLightHalfDistance(float halfDistance) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->halfDistance = std::max(halfDistance, minimumHalfDistance);

        // Update shader data.
        mtxShaderData.second.shaderData.halfDistance = this->halfDistance;

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void PointLightNode::onAfterDeserialized() {
        SpatialNode::onAfterDeserialized();

        // Make sure our intensity is in range [0.0; 1.0].
        intensity = std::clamp(intensity, 0.0F, 1.0F);

        // Make sure our half distance is not zero or negative.
        halfDistance = std::max(halfDistance, minimumHalfDistance);

#if defined(DEBUG)
        static_assert(sizeof(PointLightShaderData) == 48, "consider clamping new parameters here");
#endif
    }

    void* PointLightNode::onStartedUpdatingShaderData() {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.shaderData;
    }

    void PointLightNode::onFinishedUpdatingShaderData() { mtxShaderData.first.unlock(); }

    void PointLightNode::markShaderDataToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Make sure the slot exists.
        if (mtxShaderData.second.pPointLightArraySlot == nullptr) {
            return;
        }

        // Mark as "needs update".
        mtxShaderData.second.pPointLightArraySlot->markAsNeedsUpdate();
    }

    glm::vec3 PointLightNode::getLightColor() const { return color; }

    float PointLightNode::getLightIntensity() const { return intensity; }

    float PointLightNode::getLightHalfDistance() const { return halfDistance; }

    void PointLightNode::onWorldLocationRotationScaleChanged() {
        SpatialNode::onWorldLocationRotationScaleChanged();

        std::scoped_lock guard(mtxShaderData.first);

        // Update shader data.
        mtxShaderData.second.shaderData.position = glm::vec4(getWorldLocation(), 1.0F);

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

}
