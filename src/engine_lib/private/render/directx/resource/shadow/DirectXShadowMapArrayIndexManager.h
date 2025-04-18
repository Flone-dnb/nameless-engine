#pragma once

// Standard.
#include <memory>
#include <mutex>

// Custom.
#include "render/general/resource/shadow/ShadowMapArrayIndexManager.h"
#include "render/directx/descriptors/DirectXDescriptorHeap.h"

namespace ne {
    class ShadowMapHandle;
    class DirectXResource;
    class GpuResourceManager;

    /** Manages indices of shadows maps into a descriptor array used by shaders. */
    class DirectXShadowMapArrayIndexManager : public ShadowMapArrayIndexManager {
    public:
        DirectXShadowMapArrayIndexManager() = delete;

        DirectXShadowMapArrayIndexManager(const DirectXShadowMapArrayIndexManager&) = delete;
        DirectXShadowMapArrayIndexManager& operator=(const DirectXShadowMapArrayIndexManager&) = delete;

        DirectXShadowMapArrayIndexManager(DirectXShadowMapArrayIndexManager&&) noexcept = delete;
        DirectXShadowMapArrayIndexManager& operator=(DirectXShadowMapArrayIndexManager&&) noexcept = delete;

        /** Makes sure no descriptor is registered. */
        virtual ~DirectXShadowMapArrayIndexManager() override;

        /**
         * Creates a new index manager.
         *
         * @param pRenderer                Renderer.
         * @param pResourceManager         Resource manager.
         * @param sShaderArrayResourceName Name of the array (defined in shaders) that this manager controls.
         *
         * @return Error if something went wrong, otherwise created manager.
         */
        static std::variant<std::unique_ptr<DirectXShadowMapArrayIndexManager>, Error> create(
            Renderer* pRenderer,
            GpuResourceManager* pResourceManager,
            const std::string& sShaderArrayResourceName);

        /**
         * Returns internal SRV descriptor range to the array of shadow maps.
         *
         * @return SRV descriptor range.
         */
        inline ContinuousDirectXDescriptorRange* getSrvDescriptorRange() const { return pSrvRange.get(); }

    protected:
        /**
         * Initializes the manager except for SRV range which is expected to be initialized after
         * construction.
         *
         * @warning Only used internally, prefer to use @ref create.
         *
         * @param pRenderer                Renderer.
         * @param sShaderArrayResourceName Name of the array (defined in shaders) that this manager controls.
         */
        DirectXShadowMapArrayIndexManager(Renderer* pRenderer, const std::string& sShaderArrayResourceName);

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
        /** Called after SRV range changed its location in the heap. */
        void onSrvRangeIndicesChanged();

        /**
         * Continuous SRV descriptor range of registered shadow maps for shaders.
         *
         * @remark Always valid, never changing pointer.
         */
        std::shared_ptr<ContinuousDirectXDescriptorRange> pSrvRange;

        /** Info about shadows maps that take place in this array. */
        std::pair<std::recursive_mutex, std::unordered_set<ShadowMapHandle*>> mtxRegisteredShadowMaps;
    };
}
