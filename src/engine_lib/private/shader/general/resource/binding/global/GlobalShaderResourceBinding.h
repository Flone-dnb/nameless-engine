#pragma once

// Standard.
#include <variant>
#include <memory>
#include <optional>
#include <array>

// Custom.
#include "render/general/resource/frame/FrameResourceManager.h"
#include "misc/Error.h"

namespace ne {
    class Pipeline;
    class Renderer;
    class GpuResource;
    class GlobalShaderResourceBindingManager;

    /**
     * RAII-style type that:
     * - during construction: binds a GPU resource to all graphics pipelines that use it
     * and updates the binding if new pipelines are created or old ones are updated
     * - during destruction: removes binding from all graphics pipelines that used it.
     *
     * Used for binding GPU resources as "global" shader resources (that don't change on a per-object basis).
     */
    class GlobalShaderResourceBinding {
        // Only manager is allowed to create such objects.
        friend class GlobalShaderResourceBindingManager;

    public:
        GlobalShaderResourceBinding() = delete;

        GlobalShaderResourceBinding(const GlobalShaderResourceBinding&) = delete;
        GlobalShaderResourceBinding& operator=(const GlobalShaderResourceBinding&) = delete;

        GlobalShaderResourceBinding(GlobalShaderResourceBinding&&) = delete;
        GlobalShaderResourceBinding& operator=(GlobalShaderResourceBinding&&) = delete;

        virtual ~GlobalShaderResourceBinding() = default;

    protected:
        /**
         * Creates a new render-specific binding and registers it in the manager.
         * Also assigs this new binding to the specified GPU resource so that the binding will be
         * removed once the resources are destroyed.
         *
         * @param pRenderer            Renderer.
         * @param sShaderResourceName  Name of the shader resource (name from shader code) to bind the
         * resources.
         * @param pManager             Manager that creates this object.
         * @param vResourcesToBind     Resources to bind to pipelines. This can be the same resource for all
         * frame resources (for example a texture) or a separate resource per frame (can be used for
         * some CPU-write resources).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error> create(
            Renderer* pRenderer,
            GlobalShaderResourceBindingManager* pManager,
            const std::string& sShaderResourceName,
            const std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()>& vResourcesToBind);

        /**
         * Initializes base entity (derived types will do the binding logic).
         *
         * @remark Used internally, instead prefer to use @ref create.
         *
         * @param pManager            Manager that creates this object.
         * @param sShaderResourceName Name of the shader resource (name from shader code) to bind the
         * resources.
         * @param vResourcesToBind    Resources to bind to pipelines. This can be the same resource for all
         * frame resources (for example a texture) or a separate resource per frame (can be used for
         * some CPU-write resources).
         */
        GlobalShaderResourceBinding(
            GlobalShaderResourceBindingManager* pManager,
            const std::string& sShaderResourceName,
            const std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()>& vResourcesToBind);

        /**
         * Binds the resource to a specific pipeline or all graphics pipelines that use it.
         *
         * @param pSpecificPipeline `nullptr` if need to bind to all graphics pipelines, otherwise
         * a valid pointer to bind only to that pipeline.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindToPipelines(Pipeline* pSpecificPipeline) = 0;

        /** Should be called by derived types in their destructor to unregister the binding in the manager. */
        void unregisterBinding();

        /**
         * Returns name of the shader resource (name from shader code) to bind to.
         *
         * @return Resource name.
         */
        const std::string& getShaderResourceName() const;

        /**
         * Returns GPU resources that should be binded.
         *
         * @return GPU resources.
         */
        std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()> getBindedResources() const;

    private:
        /** Name of the shader resource (name from shader code) to bind @ref vBindedResources to. */
        const std::string sShaderResourceName;

        /**
         * Resources binded to pipelines.
         *
         * @remark This can be the same resource for all frame resources (for example a texturee) or a
         * separate resource per frame (usually used for CPU-write resources).
         */
        const std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()> vBindedResources;

        /** Manager that created this object. */
        GlobalShaderResourceBindingManager* const pManager = nullptr;
    };
}
