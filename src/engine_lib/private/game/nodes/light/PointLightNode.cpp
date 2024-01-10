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

        std::scoped_lock guard(mtxShaderData.first);

        // Create a shadow map.
        const auto pShadowMapManager =
            getGameInstance()->getWindow()->getRenderer()->getResourceManager()->getShadowMapManager();
        auto shadowMapResult = pShadowMapManager->createShadowMap(
            getNodeName(), ShadowMapType::POINT, [this](unsigned int iIndexToUse) {
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
        const auto pPointLightDataArray = pLightingShaderResourceManager->getPointLightDataArray();
        auto result = pPointLightDataArray->reserveNewSlot(
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

        for (size_t i = 0; i < mtxShaderData.second.vViewProjectionMatrixGroups.size(); i++) {
            // Reserve a slot to copy our `viewProjection` matrices
            // so that it will be available in the shaders.
            result = pLightingShaderResourceManager->getLightViewProjectionMatricesArray()->reserveNewSlot(
                this,
                sizeof(glm::mat4x4),
                [this, i]() { return onStartedUpdatingViewProjectionMatrix(i); },
                [this]() { onFinishedUpdatingViewProjectionMatrix(); });
            if (std::holds_alternative<Error>(result)) [[unlikely]] {
                auto error = std::get<Error>(std::move(result));
                error.addCurrentLocationToErrorStack();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Save received slot.
            mtxShaderData.second.vViewProjectionMatrixGroups[i].pSlot =
                std::get<std::unique_ptr<ShaderLightArraySlot>>(std::move(result));
        }
    }

    void PointLightNode::onDespawning() {
        SpatialNode::onDespawning();

        // Mark slot as unused.
        std::scoped_lock guard(mtxShaderData.first);

        // Mark light slot as unused.
        mtxShaderData.second.pPointLightArraySlot = nullptr;

        // Free shadow map.
        pShadowMapHandle = nullptr;

        // Free matrix slots.
        for (auto& group : mtxShaderData.second.vViewProjectionMatrixGroups) {
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
        this->distance = std::max(distance, minLightDistance);

        // Update shader data.
        mtxShaderData.second.shaderData.distance = this->distance;

        // Update matrices for shadow mapping.
        recalculateViewProjectionMatricesForShadowMapping();

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
        markViewProjectionMatricesToBeCopiedToGpu();
    }

    void PointLightNode::onAfterDeserialized() {
        SpatialNode::onAfterDeserialized();

        // Make sure our intensity is in range [0.0; 1.0].
        intensity = std::clamp(intensity, 0.0F, 1.0F);

        // Make sure distance is valid.
        distance = std::max(distance, minLightDistance);

#if defined(DEBUG)
        static_assert(sizeof(PointLightShaderData) == 48, "consider clamping new parameters here");
#endif
    }

    void* PointLightNode::onStartedUpdatingShaderData() {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.shaderData;
    }

    void PointLightNode::onFinishedUpdatingShaderData() { mtxShaderData.first.unlock(); }

    void* PointLightNode::onStartedUpdatingViewProjectionMatrix(size_t iMatrixIndex) {
        mtxShaderData.first.lock(); // don't unlock until finished with update

        return &mtxShaderData.second.vViewProjectionMatrixGroups[iMatrixIndex].matrix;
    }

    void PointLightNode::onFinishedUpdatingViewProjectionMatrix() { mtxShaderData.first.unlock(); }

    void PointLightNode::markShaderDataToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Make sure the slot exists.
        if (mtxShaderData.second.pPointLightArraySlot == nullptr) {
            return;
        }

        // Mark as "needs update".
        mtxShaderData.second.pPointLightArraySlot->markAsNeedsUpdate();

        // Recalculate sphere shape.
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
        recalculateViewProjectionMatricesForShadowMapping();

        // Mark updated shader data to be later copied to the GPU resource.
        markShaderDataToBeCopiedToGpu();
        markViewProjectionMatricesToBeCopiedToGpu();
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

    void PointLightNode::recalculateViewProjectionMatricesForShadowMapping() {
        std::scoped_lock guard(mtxShaderData.first);

        // Prepare some constants.
        const auto worldLocation = getWorldLocation();
        const auto farClipPlane = distance;
        const auto nearClipPlane = distance * ShadowMapManager::getVisibleDistanceToNearClipPlaneRatio();

        // Prepare a short reference.
        auto& matrixGroups = mtxShaderData.second.vViewProjectionMatrixGroups;

        // Calculate projection matrix.
        const auto projectionMatrix = glm::perspectiveLH(90.0F, 1.0F, nearClipPlane, farClipPlane);

        // +X cubemap face.
        matrixGroups[0].matrix =
            projectionMatrix *
            glm::lookAtLH(
                worldLocation, worldLocation + getWorldForwardDirection(), Globals::WorldDirection::up);

        // -X cubemap face.
        matrixGroups[1].matrix =
            projectionMatrix *
            glm::lookAtLH(
                worldLocation, worldLocation - getWorldForwardDirection(), Globals::WorldDirection::up);

        // +Y cubemap face.
        matrixGroups[2].matrix =
            projectionMatrix *
            glm::lookAtLH(
                worldLocation, worldLocation + getWorldRightDirection(), Globals::WorldDirection::up);

        // -Y cubemap face.
        matrixGroups[3].matrix =
            projectionMatrix *
            glm::lookAtLH(
                worldLocation, worldLocation - getWorldRightDirection(), Globals::WorldDirection::up);

        // +Z cubemap face.
        matrixGroups[4].matrix =
            projectionMatrix *
            glm::lookAtLH(
                worldLocation, worldLocation + getWorldUpDirection(), Globals::WorldDirection::right);

        // -Z cubemap face.
        matrixGroups[5].matrix =
            projectionMatrix *
            glm::lookAtLH(
                worldLocation, worldLocation - getWorldUpDirection(), -Globals::WorldDirection::right);
    }

    void PointLightNode::markViewProjectionMatricesToBeCopiedToGpu() {
        std::scoped_lock guard(mtxShaderData.first);

        // Make sure at least one slot exists.
        if (mtxShaderData.second.vViewProjectionMatrixGroups[0].pSlot == nullptr) {
            return; // quit, other slots also not created
        }

        // Mark as "needs update".
        for (auto& group : mtxShaderData.second.vViewProjectionMatrixGroups) {
            group.pSlot->markAsNeedsUpdate();
        }
    }

    ShadowMapHandle* PointLightNode::getShadowMapHandle() const { return pShadowMapHandle.get(); }

    unsigned int PointLightNode::getIndexIntoLightViewProjectionShaderArray(size_t iCubemapFaceIndex) {
        std::scoped_lock guard(mtxShaderData.first);

        // Prepare a short reference.
        auto& groups = mtxShaderData.second.vViewProjectionMatrixGroups;

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
            Error error(std::format(
                "expected viewProjectionMatrix slot to be valid on light node \"{}\"", getNodeName()));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Get index.
        const auto iIndex = groups[iCubemapFaceIndex].pSlot->getCurrentIndexIntoArray();

        // Make sure we won't reach uint limit.
        if (iIndex > std::numeric_limits<unsigned int>::max()) [[unlikely]] {
            Error error(std::format(
                "viewProjectionMatrix slot index on light node \"{}\" reached type limit: {}",
                getNodeName(),
                iIndex));
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Cast to uint because Vulkan and DirectX operate on `uint`s.
        return static_cast<unsigned int>(iIndex);
    }

}
