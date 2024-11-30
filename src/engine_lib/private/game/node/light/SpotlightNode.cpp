#include "game/node/light/SpotlightNode.h"

// Custom.
#include "game/Window.h"
#include "render/Renderer.h"
#include "render/general/resource/shadow/ShadowMapManager.h"
#include "render/general/resource/GpuResourceManager.h"

#include "SpotlightNode.generated_impl.h"

namespace ne {

    SpotlightNode::SpotlightNode() : SpotlightNode("Spotlight Node") {}

    SpotlightNode::SpotlightNode(const std::string& sNodeName) : SpatialNode(sNodeName) {}

    void SpotlightNode::onSpawning() {
        SpatialNode::onSpawning();

        std::scoped_lock guard(mtxShaderData.first);

        // Create a shadow map.
        const auto pShadowMapManager =
            getGameInstance()->getWindow()->getRenderer()->getResourceManager()->getShadowMapManager();
        auto shadowMapResult = pShadowMapManager->createShadowMap(
            std::format("{} shadow map", getNodeName()),
            ShadowMapType::SPOT,
            [this](unsigned int iIndexToUse) { onShadowMapArrayIndexChanged(iIndexToUse); });
        if (std::holds_alternative<Error>(shadowMapResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(shadowMapResult));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        pShadowMapHandle = std::get<std::unique_ptr<ShadowMapHandle>>(std::move(shadowMapResult));

        // Get lighting manager.
        const auto pLightingShaderResourceManager =
            getGameInstance()->getWindow()->getRenderer()->getLightingShaderResourceManager();

        // Reserve a slot in the spotlight shader data array
        // so that our parameters will be available in the shaders.
        const auto pSpotlightDataArray = pLightingShaderResourceManager->getSpotlightDataArray();
        auto result = pSpotlightDataArray->reserveNewSlot(
            this,
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

        // Update shader data.
        recalculateAndMarkShaderDataToBeCopiedToGpu();
    }

    void SpotlightNode::onDespawning() {
        SpatialNode::onDespawning();

        std::scoped_lock guard(mtxShaderData.first);

        // Mark slots as unused.
        mtxShaderData.second.pSpotlightArraySlot = nullptr;
        mtxShaderData.second.shadowPassData.pSlot = nullptr;

        // Free shadow map.
        pShadowMapHandle = nullptr;
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
        this->distance = glm::max(distance, 0.0F);

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
        distance = glm::max(distance, 0.0F);

#if defined(DEBUG)
        static_assert(sizeof(SpotlightShaderData) == 144, "consider clamping new parameters here");
#endif
    }

    void* SpotlightNode::onStartedUpdatingShaderData() {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.shaderData;
    }

    void SpotlightNode::onFinishedUpdatingShaderData() { mtxShaderData.first.unlock(); }

    void* SpotlightNode::onStartedUpdatingShadowPassData() {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.shadowPassData.shaderData;
    }

    void SpotlightNode::onFinishedUpdatingShadowPassData() { mtxShaderData.first.unlock(); }

    void SpotlightNode::recalculateAndMarkShaderDataToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Recalculate shadow mapping data.
        recalculateShadowMappingShaderData();

        // Copy up to date parameters.
        mtxShaderData.second.shaderData.position = glm::vec4(getWorldLocation(), 1.0F);
        mtxShaderData.second.shaderData.direction = glm::vec4(getWorldForwardDirection(), 0.0F);
        mtxShaderData.second.shaderData.color = glm::vec4(color, 1.0F);
        mtxShaderData.second.shaderData.intensity = intensity;
        mtxShaderData.second.shaderData.distance = distance;
        mtxShaderData.second.shaderData.cosInnerConeAngle = glm::cos(glm::radians(innerConeAngle));
        mtxShaderData.second.shaderData.cosOuterConeAngle = glm::cos(glm::radians(outerConeAngle));

        static_assert(
            maxConeAngle < 80.1F, // NOLINT
            "tan 80+ degrees will increase very fast so keep it away from 90 degrees to avoid huge cone "
            "radius");
        mtxShaderData.second.shaderData.coneBottomRadius =
            glm::tan(glm::radians(outerConeAngle)) * distance *
            1.3F; // NOLINT: TODO: multiply to avoid a rare light culling issue when viewing exactly in the
                  // direction of the spotlight (light outer cone bounds are slightly culled)

#if defined(DEBUG)
        static_assert(sizeof(SpotlightShaderData) == 144, "consider copying new parameters here");
#endif

        // Mark to be copied to the GPU.
        markShaderDataToBeCopiedToGpu();

        // Recalculate sphere shape.
        recalculateShape();
    }

    void SpotlightNode::markShaderDataToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Make sure the slot exists.
        if (mtxShaderData.second.pSpotlightArraySlot == nullptr) {
            return;
        }

        // Mark as "needs update".
        mtxShaderData.second.pSpotlightArraySlot->markAsNeedsUpdate();

        // Make sure the slot exists.
        if (mtxShaderData.second.shadowPassData.pSlot == nullptr) {
            return;
        }

        // Mark as "needs update".
        mtxShaderData.second.shadowPassData.pSlot->markAsNeedsUpdate();
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

    std::pair<std::mutex, Cone>* SpotlightNode::getShape() { return &mtxShape; }

    void SpotlightNode::recalculateShape() {
        std::scoped_lock guard(mtxShaderData.first, mtxShape.first);

        mtxShape.second.location = mtxShaderData.second.shaderData.position;
        mtxShape.second.direction = mtxShaderData.second.shaderData.direction;
        mtxShape.second.height = mtxShaderData.second.shaderData.distance;
        mtxShape.second.bottomRadius = mtxShaderData.second.shaderData.coneBottomRadius;
    }

    ShadowMapHandle* SpotlightNode::getShadowMapHandle() const { return pShadowMapHandle.get(); }

    unsigned int SpotlightNode::getIndexIntoShadowPassInfoShaderArray() {
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

    void SpotlightNode::onShadowMapArrayIndexChanged(unsigned int iNewIndexIntoArray) {
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

    void SpotlightNode::recalculateShadowMappingShaderData() {
        std::scoped_lock guard(mtxShaderData.first);

        // Prepare some constants.
        const auto worldLocation = getWorldLocation();
        const auto farClipPlane = distance;
        const auto nearClipPlane = distance * ShadowMapManager::getVisibleDistanceToNearClipPlaneRatio();

        // Calculate view matrix.
        const auto viewMatrix =
            glm::lookAtLH(worldLocation, worldLocation + getWorldForwardDirection(), getWorldUpDirection());

        // Prepare FOV for shadow map capture.
        static_assert(maxConeAngle <= 90.0F, "change FOV for shadow map capture"); // NOLINT
        const auto fovY = glm::radians(outerConeAngle * 2.0F); // x2 to convert [0..90] degree to [0..180] FOV

        // Calculate projection matrix.
        mtxShaderData.second.shaderData.viewProjectionMatrix =
            glm::perspectiveLH(fovY, 1.0F, nearClipPlane, farClipPlane) * viewMatrix;

        // Create a short reference.
        auto& shadowPassData = mtxShaderData.second.shadowPassData.shaderData;

        // Update shadow pass data.
        shadowPassData.viewProjectionMatrix = mtxShaderData.second.shaderData.viewProjectionMatrix;
        shadowPassData.position = glm::vec4(getWorldLocation(), 1.0F);
    }

}
