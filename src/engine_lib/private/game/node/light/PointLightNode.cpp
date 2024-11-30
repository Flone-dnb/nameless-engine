#include "game/node/light/PointLightNode.h"

// Custom.
#include "game/Window.h"
#include "render/Renderer.h"
#include "render/general/resource/GpuResourceManager.h"

#include "PointLightNode.generated_impl.h"

namespace ne {

    PointLightNode::PointLightNode() : PointLightNode("Point Light Node") {}

    PointLightNode::PointLightNode(const std::string& sNodeName) : SpatialNode(sNodeName) {}

    void PointLightNode::onSpawning() {
        SpatialNode::onSpawning();

        std::scoped_lock guard(mtxShaderData.first);

        // Create a shadow map.
        const auto pShadowMapManager =
            getGameInstance()->getWindow()->getRenderer()->getResourceManager()->getShadowMapManager();
        auto shadowMapResult = pShadowMapManager->createShadowMap(
            std::format("{} shadow map", getNodeName()),
            ShadowMapType::POINT,
            [this](unsigned int iIndexToUse) { onShadowMapArrayIndexChanged(iIndexToUse); });
        if (std::holds_alternative<Error>(shadowMapResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(shadowMapResult));
            error.addCurrentLocationToErrorStack();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }
        pShadowMapHandle = std::get<std::unique_ptr<ShadowMapHandle>>(std::move(shadowMapResult));

        // Copy up to date parameters.
        mtxShaderData.second.shaderData.position = glm::vec4(getWorldLocation(), 1.0F);
        mtxShaderData.second.shaderData.color = glm::vec4(color, 1.0F);
        mtxShaderData.second.shaderData.intensity = intensity;
        mtxShaderData.second.shaderData.distance = distance;
#if defined(DEBUG)
        static_assert(sizeof(PointLightShaderData) == 48, "consider copying new parameters here");
#endif

        // Recalculate sphere shape.
        recalculateShape();

        // Get lighting manager.
        const auto pLightingShaderResourceManager =
            getGameInstance()->getWindow()->getRenderer()->getLightingShaderResourceManager();

        // Reserve a slot in the point light shader data array
        // so that our parameters will be available in the shaders.
        auto result = pLightingShaderResourceManager->getPointLightDataArray()->reserveNewSlot(
            this,
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

        for (size_t i = 0; i < mtxShaderData.second.vShadowPassDataGroup.size(); i++) {
            // Reserve a slot to copy our shadow pass data.
            result = pLightingShaderResourceManager->getShadowPassLightInfoArray()->reserveNewSlot(
                this,
                sizeof(ShadowPassLightShaderInfo),
                [this, i]() { return onStartedUpdatingShadowPassData(i); },
                [this]() { onFinishedUpdatingShadowPassData(); });
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Save received slot.
            mtxShaderData.second.vShadowPassDataGroup[i].pSlot =
                std::get<std::unique_ptr<ShaderLightArraySlot>>(std::move(result));
        }
    }

    void PointLightNode::onDespawning() {
        SpatialNode::onDespawning();

        // Mark slot as unused.
        std::scoped_lock guard(mtxShaderData.first);

        // Mark light slots as unused.
        mtxShaderData.second.pPointLightArraySlot = nullptr;

        // Free shadow map.
        pShadowMapHandle = nullptr;

        // Free matrix slots.
        for (auto& group : mtxShaderData.second.vShadowPassDataGroup) {
            group.pSlot = nullptr;
        }
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

    void PointLightNode::setLightDistance(float distance) {
        std::scoped_lock guard(mtxShaderData.first);

        // Save new parameter.
        this->distance = std::max(distance, 0.0F);

        // Update shader data.
        mtxShaderData.second.shaderData.distance = this->distance;

        // Update matrices for shadow mapping.
        recalculateShadowPassShaderData();

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
        markShadowPassDataToBeCopiedToGpu();
    }

    void PointLightNode::onAfterDeserialized() {
        SpatialNode::onAfterDeserialized();

        // Make sure our intensity is in range [0.0; 1.0].
        intensity = std::clamp(intensity, 0.0F, 1.0F);

        // Make sure distance is valid.
        distance = std::max(distance, 0.0F);

#if defined(DEBUG)
        static_assert(sizeof(PointLightShaderData) == 48, "consider clamping new parameters here");
#endif
    }

    void* PointLightNode::onStartedUpdatingShaderData() {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.shaderData;
    }

    void PointLightNode::onFinishedUpdatingShaderData() { mtxShaderData.first.unlock(); }

    void* PointLightNode::onStartedUpdatingShadowPassData(size_t iCubemapFaceIndex) {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.vShadowPassDataGroup[iCubemapFaceIndex].shaderData;
    }

    void PointLightNode::onFinishedUpdatingShadowPassData() { mtxShaderData.first.unlock(); }

    void PointLightNode::markShaderDataToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Make sure the slot exists.
        if (mtxShaderData.second.pPointLightArraySlot == nullptr) {
            return;
        }

        // Mark as "needs update".
        mtxShaderData.second.pPointLightArraySlot->markAsNeedsUpdate();

        // Recalculate sphere shape and use updated shader data.
        recalculateShape();
    }

    glm::vec3 PointLightNode::getLightColor() const { return color; }

    float PointLightNode::getLightIntensity() const { return intensity; }

    float PointLightNode::getLightDistance() const { return distance; }

    void PointLightNode::onWorldLocationRotationScaleChanged() {
        SpatialNode::onWorldLocationRotationScaleChanged();

        std::scoped_lock guard(mtxShaderData.first);

        // Update shader data.
        mtxShaderData.second.shaderData.position = glm::vec4(getWorldLocation(), 1.0F);

        // Update matrices for shadow mapping.
        recalculateShadowPassShaderData();

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
        markShadowPassDataToBeCopiedToGpu();
    }

    void PointLightNode::recalculateShape() {
        std::scoped_lock guard(mtxShaderData.first, mtxShape.first);

        mtxShape.second.center = mtxShaderData.second.shaderData.position;
        mtxShape.second.radius = mtxShaderData.second.shaderData.distance;
    }

    std::pair<std::mutex, Sphere>* PointLightNode::getShape() { return &mtxShape; }

    void PointLightNode::onShadowMapArrayIndexChanged(unsigned int iNewIndexIntoArray) {
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

    void PointLightNode::recalculateShadowPassShaderData() {
        std::scoped_lock guard(mtxShaderData.first);

        // Prepare some constants.
        const auto worldLocation = getWorldLocation();
        const auto farClipPlane = distance; // the fact that far clip plane is lit distance is used in shaders
        const auto nearClipPlane = distance * ShadowMapManager::getVisibleDistanceToNearClipPlaneRatio();

        // Prepare a short reference.
        auto& dataGroups = mtxShaderData.second.vShadowPassDataGroup;

        // Calculate projection matrix.
        const auto projectionMatrix =
            glm::perspectiveLH(glm::radians(90.0F), 1.0F, nearClipPlane, farClipPlane);

        // Save world location.
        for (auto& data : dataGroups) {
            data.shaderData.position = glm::vec4(worldLocation, 1.0F);
        }

        // Update matrices.

        // Cubemap face 0:
        // +X axis should be forward (in our case it's forward)
        // +Y axis should be up (in our case it's right)
        dataGroups[0].shaderData.viewProjectionMatrix =
            projectionMatrix * glm::lookAtLH(
                                   worldLocation,
                                   worldLocation + Globals::WorldDirection::forward,
                                   Globals::WorldDirection::right);

        // Cubemap face 1:
        // -X axis should be forward (in our case it's minus forward)
        // +Y axis should be up (in our case it's right)
        dataGroups[1].shaderData.viewProjectionMatrix =
            projectionMatrix * glm::lookAtLH(
                                   worldLocation,
                                   worldLocation - Globals::WorldDirection::forward,
                                   Globals::WorldDirection::right);

        // Cubemap face 2:
        // +Y axis should be forward (in our case it's right)
        // -Z axis should be up (in our case it's minus up)
        dataGroups[2].shaderData.viewProjectionMatrix =
            projectionMatrix *
            glm::lookAtLH(
                worldLocation, worldLocation + Globals::WorldDirection::right, -Globals::WorldDirection::up);

        // Cubemap face 3:
        // -Y axis should be forward (in our case it's minus right)
        // +Z axis should be up (in our case it's up)
        dataGroups[3].shaderData.viewProjectionMatrix =
            projectionMatrix *
            glm::lookAtLH(
                worldLocation, worldLocation - Globals::WorldDirection::right, Globals::WorldDirection::up);

        // Cubemap face 4:
        // +Z axis should be forward (in our case it's up)
        // +Y axis should be up (in our case it's right)
        dataGroups[4].shaderData.viewProjectionMatrix =
            projectionMatrix *
            glm::lookAtLH(
                worldLocation, worldLocation + Globals::WorldDirection::up, Globals::WorldDirection::right);

        // Cubemap face 5:
        // -Z axis should be forward (in our case it's minus up)
        // +Y axis should be up (in our case it's right)
        dataGroups[5].shaderData.viewProjectionMatrix = // NOLINT
            projectionMatrix *
            glm::lookAtLH(
                worldLocation, worldLocation - Globals::WorldDirection::up, Globals::WorldDirection::right);
    }

    void PointLightNode::markShadowPassDataToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Make sure at least one slot exists.
        if (mtxShaderData.second.vShadowPassDataGroup[0].pSlot == nullptr) {
            return; // quit, other slots also not created
        }

        // Mark as "needs update".
        for (auto& group : mtxShaderData.second.vShadowPassDataGroup) {
            group.pSlot->markAsNeedsUpdate();
        }
    }

    ShadowMapHandle* PointLightNode::getShadowMapHandle() const { return pShadowMapHandle.get(); }

    unsigned int PointLightNode::getIndexIntoShadowPassInfoShaderArray(size_t iCubemapFaceIndex) {
        std::scoped_lock guard(mtxShaderData.first);

        // Prepare a short reference.
        auto& groups = mtxShaderData.second.vShadowPassDataGroup;

        // Make sure index is not out of bounds.
        if (iCubemapFaceIndex >= groups.size()) [[unlikely]] {
            Error error(std::format(
                "the specified cubemap face index {} is invalid (light node \"{}\")",
                iCubemapFaceIndex,
                getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Make sure the slot exists.
        if (groups[iCubemapFaceIndex].pSlot == nullptr) [[unlikely]] {
            Error error(std::format("expected slot to be valid on light node \"{}\"", getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get index.
        const auto iIndex = groups[iCubemapFaceIndex].pSlot->getCurrentIndexIntoArray();

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
