#include "game/nodes/light/DirectionalLightNode.h"

// Custom.
#include "game/Window.h"
#include "render/Renderer.h"
#include "render/general/resources/shadow/ShadowMapManager.h"

#include "DirectionalLightNode.generated_impl.h"

namespace ne {

    DirectionalLightNode::DirectionalLightNode() : DirectionalLightNode("Directional Light Node") {}

    DirectionalLightNode::DirectionalLightNode(const std::string& sNodeName) : SpatialNode(sNodeName) {}

    void DirectionalLightNode::onAfterDeserialized() {
        SpatialNode::onAfterDeserialized();

        // Make sure our intensity is in range [0.0; 1.0].
        intensity = std::clamp(intensity, 0.0F, 1.0F);

#if defined(DEBUG)
        static_assert(sizeof(DirecionalLightShaderData) == 48, "consider clamping new parameters here");
#endif
    }

    void* DirectionalLightNode::onStartedUpdatingShaderData() {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.shaderData;
    }

    void DirectionalLightNode::onFinishedUpdatingShaderData() { mtxShaderData.first.unlock(); }

    void DirectionalLightNode::markShaderDataToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Make sure the slot exists.
        if (mtxShaderData.second.pDirectionalLightArraySlot == nullptr) {
            return;
        }

        // Mark as "needs update".
        mtxShaderData.second.pDirectionalLightArraySlot->markAsNeedsUpdate();
    }

    void DirectionalLightNode::onDespawning() {
        SpatialNode::onDespawning();

        std::scoped_lock guard(mtxShaderData.first);

        // Mark light slot as unused.
        mtxShaderData.second.pDirectionalLightArraySlot = nullptr;

        // Free shadow map.
        pShadowMapHandle = nullptr;
    }

    void DirectionalLightNode::onSpawning() {
        SpatialNode::onSpawning();

        // Prepare shader data.
        std::scoped_lock guard(mtxShaderData.first);

        // Create a shadow map.
        const auto pShadowMapManager =
            getGameInstance()->getWindow()->getRenderer()->getResourceManager()->getShadowMapManager();
        auto shadowMapResult = pShadowMapManager->createShadowMap(
            getNodeName(), ShadowMapType::DIRECTIONAL, [this](unsigned int iIndexToUse) {
                onShadowMapArrayIndexChanged(iIndexToUse);
            });
        if (std::holds_alternative<Error>(shadowMapResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(shadowMapResult));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        pShadowMapHandle = std::get<std::unique_ptr<ShadowMapHandle>>(std::move(shadowMapResult));

        // Copy up to date parameters.
        mtxShaderData.second.shaderData.direction = glm::vec4(getWorldForwardDirection(), 0.0F);
        mtxShaderData.second.shaderData.color = glm::vec4(color, 1.0F);
        mtxShaderData.second.shaderData.intensity = intensity;
#if defined(DEBUG)
        static_assert(sizeof(DirecionalLightShaderData) == 48, "consider copying new parameters here");
#endif

        // Reserve a slot in the directional light shader data array
        // so that our parameters will be available in the shaders.
        const auto pDirectionalLightArray = getGameInstance()
                                                ->getWindow()
                                                ->getRenderer()
                                                ->getLightingShaderResourceManager()
                                                ->getDirectionalLightDataArray();
        auto result = pDirectionalLightArray->reserveNewSlot(
            sizeof(DirecionalLightShaderData),
            [this]() { return onStartedUpdatingShaderData(); },
            [this]() { onFinishedUpdatingShaderData(); });
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Save received slot.
        mtxShaderData.second.pDirectionalLightArraySlot =
            std::get<std::unique_ptr<ShaderLightArraySlot>>(std::move(result));
    }

    glm::vec3 DirectionalLightNode::getLightColor() const { return color; }

    float DirectionalLightNode::getLightIntensity() const { return intensity; }

    void DirectionalLightNode::setLightIntensity(float intensity) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->intensity = std::clamp(intensity, 0.0F, 1.0F);

        // Update shader data.
        mtxShaderData.second.shaderData.intensity = this->intensity;

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void DirectionalLightNode::setLightColor(const glm::vec3& color) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->color = color;

        // Update shader data.
        mtxShaderData.second.shaderData.color = glm::vec4(this->color, 1.0F);

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void DirectionalLightNode::onWorldLocationRotationScaleChanged() {
        SpatialNode::onWorldLocationRotationScaleChanged();

        std::scoped_lock guard(mtxShaderData.first);

        // Update shader data.
        mtxShaderData.second.shaderData.direction = glm::vec4(getWorldForwardDirection(), 0.0F);

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void DirectionalLightNode::onShadowMapArrayIndexChanged(unsigned int iNewIndexIntoArray) {
        std::scoped_lock guard(mtxShaderData.first);

        // Self check: make sure shadow map handle is valid.
        if (pShadowMapHandle == nullptr) [[unlikely]] {
            Error error(std::format(
                "node \"{}\" shadow map handle is cleared but shadow map index callback was triggered",
                getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Update shader data.
        mtxShaderData.second.shaderData.iShadowMapIndex = iNewIndexIntoArray;

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

}
