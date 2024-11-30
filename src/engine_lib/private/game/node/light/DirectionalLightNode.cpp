#include "game/node/light/DirectionalLightNode.h"

// Custom.
#include "game/Window.h"
#include "render/Renderer.h"
#include "render/general/resource/shadow/ShadowMapManager.h"
#include "render/general/resource/GpuResourceManager.h"

#include "DirectionalLightNode.generated_impl.h"

namespace ne {

    DirectionalLightNode::DirectionalLightNode() : DirectionalLightNode("Directional Light Node") {}

    DirectionalLightNode::DirectionalLightNode(const std::string& sNodeName) : SpatialNode(sNodeName) {}

    void DirectionalLightNode::onAfterDeserialized() {
        SpatialNode::onAfterDeserialized();

        // Make sure our intensity is in range [0.0; 1.0].
        intensity = std::clamp(intensity, 0.0F, 1.0F);

#if defined(DEBUG)
        static_assert(sizeof(DirecionalLightShaderData) == 112, "consider clamping new parameters here");
#endif
    }

    void* DirectionalLightNode::onStartedUpdatingShaderData() {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.shaderData;
    }

    void DirectionalLightNode::onFinishedUpdatingShaderData() { mtxShaderData.first.unlock(); }

    void* DirectionalLightNode::onStartedUpdatingShadowPassData() {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.shadowPassData.shaderData;
    }

    void DirectionalLightNode::onFinishedUpdatingShadowPassData() { mtxShaderData.first.unlock(); }

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

        // Mark slots as unused.
        mtxShaderData.second.pDirectionalLightArraySlot = nullptr;
        mtxShaderData.second.shadowPassData.pSlot = nullptr;

        // Free shadow map.
        pShadowMapHandle = nullptr;
    }

    void DirectionalLightNode::onSpawning() {
        SpatialNode::onSpawning();

        std::scoped_lock guard(mtxShaderData.first);

        // Create a shadow map.
        const auto pShadowMapManager =
            getGameInstance()->getWindow()->getRenderer()->getResourceManager()->getShadowMapManager();
        auto shadowMapResult = pShadowMapManager->createShadowMap(
            std::format("{} shadow map", getNodeName()),
            ShadowMapType::DIRECTIONAL,
            [this](unsigned int iIndexToUse) { onShadowMapArrayIndexChanged(iIndexToUse); });
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
        recalculateShadowMappingShaderData();
#if defined(DEBUG)
        static_assert(sizeof(DirecionalLightShaderData) == 112, "consider copying new parameters here");
#endif

        // Get lighting manager.
        const auto pLightingShaderResourceManager =
            getGameInstance()->getWindow()->getRenderer()->getLightingShaderResourceManager();

        // Reserve a slot in the directional light array
        // so that our parameters will be available in the shaders.
        const auto pDirectionalLightArray = pLightingShaderResourceManager->getDirectionalLightDataArray();
        auto result = pDirectionalLightArray->reserveNewSlot(
            this,
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

        // Reserve a slot to copy our shadow pass data.
        result = pLightingShaderResourceManager->getShadowPassLightInfoArray()->reserveNewSlot(
            this,
            sizeof(ShadowPassLightShaderInfo),
            [this]() { return onStartedUpdatingShadowPassData(); },
            [this]() { onFinishedUpdatingShadowPassData(); });
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Save received slot.
        mtxShaderData.second.shadowPassData.pSlot =
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

        // Update direction for shaders.
        mtxShaderData.second.shaderData.direction = glm::vec4(getWorldForwardDirection(), 0.0F);

        // Update shadow pass data.
        recalculateShadowMappingShaderData();

        // Mark matrices and shader data to be copied to the GPU.
        markShadowPassDataToBeCopiedToGpu();
        markShaderDataToBeCopiedToGpu();
    }

    void DirectionalLightNode::onShadowMapArrayIndexChanged(unsigned int iNewIndexIntoArray) {
        std::scoped_lock guard(mtxShaderData.first);

        // Self check: make sure we are spawned.
        if (!isSpawned()) [[unlikely]] {
            Error error(std::format(
                "shadow map array index callback is triggered on node \"{}\" while it's not spawned",
                getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Shadow map handle will be invalid the first time this function is called
        // (we will receive initial index into the array).

        // Update shader data.
        mtxShaderData.second.shaderData.iShadowMapIndex = iNewIndexIntoArray;

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
    }

    void DirectionalLightNode::recalculateShadowMappingShaderData() {
        std::scoped_lock guard(mtxShaderData.first);

        // Prepare some constants.
        const auto worldHalfSize = static_cast<float>(getGameInstance()->getWorldSize()) / 2.0F;
        const auto lookAtWorldPosition = glm::vec3(0.0F, 0.0F, 0.0F);

        // Calculate position for shadow capture
        // (move light to `worldHalfSize * 2` to make near Z pretty far from view space origin,
        // see below how we set near/far clip planes according to world bounds).
        const auto shadowMappingLightWorldPosition =
            -getWorldForwardDirection() * glm::vec3(worldHalfSize * 2.0F);

        // Calculate view matrix.
        const auto viewMatrix =
            glm::lookAtLH(shadowMappingLightWorldPosition, lookAtWorldPosition, Globals::WorldDirection::up);

        // Transform world look at position to light's view space.
        const auto lookAtViewPosition = viewMatrix * glm::vec4(lookAtWorldPosition, 1.0F);

        // Calculate orthographic frustum planes (bounds) in light's view space.
        const auto frustumLeft = lookAtViewPosition.x - worldHalfSize;
        const auto frustumRight = lookAtViewPosition.x + worldHalfSize;
        const auto frustumBottom = lookAtViewPosition.y - worldHalfSize;
        const auto frustumTop = lookAtViewPosition.y + worldHalfSize;
        const auto frustumNear = lookAtViewPosition.z - worldHalfSize;
        const auto frustumFar = lookAtViewPosition.z + worldHalfSize;

        // Calculate projection matrix.
        mtxShaderData.second.shaderData.viewProjectionMatrix =
            glm::orthoLH(frustumLeft, frustumRight, frustumBottom, frustumTop, frustumNear, frustumFar) *
            viewMatrix;

        // Create a short reference.
        auto& shaderPassData = mtxShaderData.second.shadowPassData.shaderData;

        // Update shadow pass data.
        shaderPassData.viewProjectionMatrix = mtxShaderData.second.shaderData.viewProjectionMatrix;
        shaderPassData.position = glm::vec4(getWorldLocation(), 1.0F);
    }

    void DirectionalLightNode::markShadowPassDataToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Make sure the slot exists.
        if (mtxShaderData.second.shadowPassData.pSlot == nullptr) {
            return;
        }

        // Mark as "needs update".
        mtxShaderData.second.shadowPassData.pSlot->markAsNeedsUpdate();
    }

    ShadowMapHandle* DirectionalLightNode::getShadowMapHandle() const { return pShadowMapHandle.get(); }

    unsigned int DirectionalLightNode::getIndexIntoShadowPassInfoShaderArray() {
        std::scoped_lock guard(mtxShaderData.first);

        // Make sure the slot exists.
        if (mtxShaderData.second.shadowPassData.pSlot == nullptr) [[unlikely]] {
            Error error(std::format("expected slot to be valid on light node \"{}\"", getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get index.
        const auto iIndex = mtxShaderData.second.shadowPassData.pSlot->getCurrentIndexIntoArray();

        // Make sure we won't reach uint limit.
        if (iIndex > std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            Error error(
                std::format("slot index on light node \"{}\" reached type limit: {}", getNodeName(), iIndex));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Cast to uint because Vulkan and DirectX operate on `uint`s.
        return static_cast<unsigned int>(iIndex);
    }

}
