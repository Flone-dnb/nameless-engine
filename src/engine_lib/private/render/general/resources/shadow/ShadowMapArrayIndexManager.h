#pragma once

// Standard.
#include <optional>
#include <variant>
#include <memory>

// Custom.
#include "misc/Error.h"

namespace ne {
    class ShadowMapHandle;
    class Pipeline;
    class Renderer;

    /** Manages indices of shadows maps into a descriptor array used by shaders. */
    class ShadowMapArrayIndexManager {
        // This class is expected to be used by shadow map manager.
        friend class ShadowMapManager;

    public:
        ShadowMapArrayIndexManager() = delete;

        ShadowMapArrayIndexManager(const ShadowMapArrayIndexManager&) = delete;
        ShadowMapArrayIndexManager& operator=(const ShadowMapArrayIndexManager&) = delete;

        ShadowMapArrayIndexManager(ShadowMapArrayIndexManager&&) noexcept = delete;
        ShadowMapArrayIndexManager& operator=(ShadowMapArrayIndexManager&&) noexcept = delete;

        virtual ~ShadowMapArrayIndexManager() = default;

        /**
         * Creates a new renderer-specific index manager.
         *
         * @param pRenderer                Renderer.
         * @param sShaderArrayResourceName Name of the array (defined in shaders) that this manager controls.
         *
         * @return Error if something went wrong, otherwise created manager.
         */
        static std::variant<std::unique_ptr<ShadowMapArrayIndexManager>, Error>
        create(Renderer* pRenderer, const std::string& sShaderArrayResourceName);

    protected:
        /**
         * Initializes object.
         *
         * @param pRenderer                Renderer.
         * @param sShaderArrayResourceName Name of the array (defined in shaders) that this manager controls.
         */
        ShadowMapArrayIndexManager(Renderer* pRenderer, const std::string& sShaderArrayResourceName);

        /**
         * Usually called by derived classes to notify some shadow map handle that its descriptor array
         * index was changed (because base index manager class is a friend class to the shadow map handle).
         *
         * @param pShadowMapHandle Shadow map handle to notify.
         * @param iNewArrayIndex   New array index for this shadow map.
         */
        static void changeShadowMapArrayIndex(ShadowMapHandle* pShadowMapHandle, unsigned int iNewArrayIndex);

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
        [[nodiscard]] virtual std::optional<Error> registerShadowMap(ShadowMapHandle* pShadowMapHandle) = 0;

        /**
         * Unregisters a shadow map and frees its index into a descriptor array to be used by others.
         *
         * @param pShadowMapHandle Shadow map to unregister.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> unregisterShadowMap(ShadowMapHandle* pShadowMapHandle) = 0;

        /**
         * Looks if the specified pipeline uses shadow maps and if uses binds shadow maps to the pipeline.
         *
         * @param pPipeline Pipeline to bind shadow maps to.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindShadowMapsToPipeline(Pipeline* pPipeline) = 0;

        /**
         * Goes through all graphics pipelines ad binds shadow maps to pipelines that use them.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindShadowMapsToAllPipelines() = 0;

        /**
         * Returns name of the array (defined in shaders) that this manager controls.
         *
         * @return Array name.
         */
        std::string getShaderArrayResourceName();

        /**
         * Returns renderer.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Renderer.
         */
        Renderer* getRenderer() const;

    private:
        /** Do not delete (free). Renderer. */
        Renderer* const pRenderer = nullptr;

        /** Name of the array (defined in shaders) that this manager controls. */
        const std::string sShaderArrayResourceName;
    };
}
