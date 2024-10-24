#pragma once

// Standard.
#include <unordered_map>

// Custom.
#include "render/general/resource/shadow/ShadowMapArrayIndexManager.h"
#include "shader/general/resource/ShaderArrayIndexManager.h"
#include "render/general/resource/frame/FrameResourceManager.h"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    class VulkanPipeline;

    /** Manages indices of shadows maps into a descriptor array used by shaders. */
    class VulkanShadowMapArrayIndexManager : public ShadowMapArrayIndexManager {
    public:
        VulkanShadowMapArrayIndexManager() = delete;

        VulkanShadowMapArrayIndexManager(const VulkanShadowMapArrayIndexManager&) = delete;
        VulkanShadowMapArrayIndexManager& operator=(const VulkanShadowMapArrayIndexManager&) = delete;

        VulkanShadowMapArrayIndexManager(VulkanShadowMapArrayIndexManager&&) noexcept = delete;
        VulkanShadowMapArrayIndexManager& operator=(VulkanShadowMapArrayIndexManager&&) noexcept = delete;

        virtual ~VulkanShadowMapArrayIndexManager() override;

        /**
         * Initializes the manager.
         *
         * @param pRenderer                Renderer.
         * @param sShaderArrayResourceName Name of the array (defined in shaders) that this manager controls.
         */
        VulkanShadowMapArrayIndexManager(Renderer* pRenderer, const std::string& sShaderArrayResourceName);

    protected:
        /**
         * Reserves an index into a descriptor array for the shadow map resource of the specified handle
         * and bind internal GPU shadow map resource (if the handle) to that descriptor.
         *
         * @remark Use @ref unregisterShadowMapResource to unregister it later (must be done before this
         * manager is destroyed) when shadow map is being destroyed.
         *
         * @param pShadowMapHandle Shadow map to register.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        registerShadowMapResource(ShadowMapHandle* pShadowMapHandle) override;

        /**
         * Unregisters a shadow map and frees its index into a descriptor array to be used by others.
         *
         * @param pShadowMapHandle Shadow map to unregister.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        unregisterShadowMapResource(ShadowMapHandle* pShadowMapHandle) override;

        /**
         * Looks if the specified pipeline uses shadow maps and if uses binds shadow maps to the pipeline.
         *
         * @param pPipeline Pipeline to bind shadow maps to.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindShadowMapsToPipeline(Pipeline* pPipeline) override;

        /**
         * Goes through all graphics pipelines ad binds shadow maps to pipelines that use them.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindShadowMapsToAllPipelines() override;

    private:
        /** Groups mutex guarded data. */
        struct InternalData {
            /**
             * Actual index manager.
             *
             * @remark We don't use index managers from specific pipelines because in the case of shadow maps
             * all index managers related to shadow map shader resources will (and should) store the same
             * indices, so in order to not duplicate this work (and in order to not store N unique ptr array
             * index objects where N is the number of pipelines for just one shadow map) we just use one index
             * manager located here.
             */
            std::unique_ptr<ShaderArrayIndexManager> pIndexManager;

            /** Pairs of "shadow map" - "index that this shadow map takes". */
            std::unordered_map<ShadowMapHandle*, std::unique_ptr<ShaderArrayIndex>> registeredShadowMaps;
        };

        /**
         * Goes through all graphics pipelines ad binds shadow map(s) to pipelines that use them.
         *
         * @param pOnlyBindThisShadowMap If `nullptr` then binds all shadow maps from @ref mtxInternalData,
         * otherwise only binds the specified already registered (!) shadow map to all pipelines that
         * reference it.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        bindShadowMapsToAllPipelines(ShadowMapHandle* pOnlyBindThisShadowMap);

        /**
         * Looks if the specified pipeline uses shadow map(s) and if uses binds shadow map(s) to the pipeline.
         *
         * @param pPipeline              Pipeline to bind shadow maps to.
         * @param pOnlyBindThisShadowMap If `nullptr` then binds all shadow maps from @ref mtxInternalData,
         * otherwise only binds the specified already registered (!) shadow map to the specified pipelines if
         * it references it.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error>
        bindShadowMapsToPipeline(Pipeline* pPipeline, ShadowMapHandle* pOnlyBindThisShadowMap);

        /**
         * Binds the specified shadow map to the specified pipeline.
         *
         * @param pShadowMapHandle        Handle to shadow map to bind.
         * @param pPipeline               Pipeline to bind the shadow map to.
         * @param pSampler                Texture sampler to use.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindShadowMapToPipelineIfUsed(
            ShadowMapHandle* pShadowMapHandle, VulkanPipeline* pPipeline, VkSampler pSampler);

        /** Mutex guarded internal data. */
        std::pair<std::recursive_mutex, InternalData> mtxInternalData;
    };
}
