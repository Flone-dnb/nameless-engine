#pragma once

// Standard.
#include <memory>
#include <mutex>

// Custom.
#include "render/general/resources/shadow/ShadowMapArrayIndexManager.h"
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
         * @param pRenderer  Renderer.
         * @param sArrayName Name of the array that this manager controls, used for logging.
         *
         * @return Error if something went wrong, otherwise created manager.
         */
        static std::variant<std::unique_ptr<DirectXShadowMapArrayIndexManager>, Error>
        create(Renderer* pRenderer, const std::string& sArrayName);

    protected:
        /**
         * Initializes the manager except for SRV range which is expected to be initialized after
         * construction.
         *
         * @warning Only used internally, prefer to use @ref create.
         *
         * @param pRenderer  Renderer.
         * @param sArrayName Name of the array that this manager controls, used for logging.
         */
        DirectXShadowMapArrayIndexManager(Renderer* pRenderer, const std::string& sArrayName);

        /**
         * Binds DSV and SRV (SRV from continuous descriptor range) to the specified shadow map and reserves
         * an index into a descriptor array for it.
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
            /** Continuous SRV descriptor range for registered shadow maps. */
            std::unique_ptr<ContinuousDirectXDescriptorRange> pSrvRange;

            /** Shadows maps that take place in this array. */
            std::unordered_set<ShadowMapHandle*> registeredShadowMaps;
        };

        /**
         * Calculates offset of the SRV descriptor (of the specified resource) from the start of the
         * continuous SRV descriptor range that this manager stores.
         *
         * @param pResource Resource with binded SRV descriptor.
         *
         * @return Error if something went wrong, otherwise offset of the SRV descriptor from range start.
         */
        std::variant<unsigned int, Error> getSrvDescriptorOffsetFromRangeStart(DirectXResource* pResource);

        /** Called after SRV range changed its location in the heap. */
        void onSrvRangeIndicesChanged();

        /** Mutex guarded internal data. */
        std::pair<std::recursive_mutex, InternalData> mtxInternalData;
    };
}
