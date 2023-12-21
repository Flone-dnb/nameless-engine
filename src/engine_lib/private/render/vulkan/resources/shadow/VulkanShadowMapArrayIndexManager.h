#pragma once

// Standard.
#include <unordered_map>

// Custom.
#include "render/general/resources/shadow/ShadowMapArrayIndexManager.h"
#include "shader/general/resources/ShaderBindlessArrayIndexManager.h"

namespace ne {
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
         * Registers a shadow map and reserves an index into a descriptor array for it.
         *
         * @remark Use @ref unregisterShadowMap to unregister it later (must be done before this manager is
         * destroyed) when shadow map is being destroyed.
         *
         * @param pShadowMapHandle Shadow map to register.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        registerShadowMap(ShadowMapHandle* pShadowMapHandle) override;

        /**
         * Unregisters a shadow map and frees its index into a descriptor array to be used by others.
         *
         * @param pShadowMapHandle Shadow map to unregister.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        unregisterShadowMap(ShadowMapHandle* pShadowMapHandle) override;

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
            std::unique_ptr<ShaderBindlessArrayIndexManager> pIndexManager;

            /** Pairs of "shadow map" - "index that this shadow map takes". */
            std::unordered_map<ShadowMapHandle*, std::unique_ptr<BindlessArrayIndex>> registeredShadowMaps;
        };

        /** Mutex guarded internal data. */
        std::pair<std::recursive_mutex, InternalData> mtxInternalData;
    };
}
