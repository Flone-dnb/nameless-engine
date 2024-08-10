#pragma once

// Standard.
#include <optional>

// Custom.
#include "shader/general/resources/GlobalShaderResourceBinding.h"

// External.
#include "vulkan/vulkan_core.h"

namespace ne {
    class GpuResource;

    /**
     * Used for binding GPU resources as "global" GLSL shader resources (that don't change on a per-object
     * basis).
     */
    class GlslGlobalShaderResourceBinding : public GlobalShaderResourceBinding {
        // Only base class is allowed to create such objects.
        friend class GlobalShaderResourceBinding;

    public:
        GlslGlobalShaderResourceBinding() = delete;

        virtual ~GlslGlobalShaderResourceBinding() override;

    protected:
        /**
         * Initializes a new object, does not trigger @ref bindToPipelines.
         *
         * @param pManager            Manager that creates this object.
         * @param sShaderResourceName Name of the shader resource (name from shader code) to bind the
         * resources.
         * @param vResourcesToBind    Resources to bind to pipelines. This can be the same resource for all
         * frame resources (for example a texture) or a separate resource per frame (can be used for
         * some CPU-write resources).
         */
        GlslGlobalShaderResourceBinding(
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
        [[nodiscard]] virtual std::optional<Error> bindToPipelines(Pipeline* pSpecificPipeline) override;
    };
}
