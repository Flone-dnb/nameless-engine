#pragma once

// Standard.
#include <optional>
#include <functional>

// Custom.
#include "shader/general/resource/binding/global/GlobalShaderResourceBinding.h"
#include "render/directx/descriptors/DirectXDescriptorType.hpp"

// External.
#include "vulkan/vulkan_core.h"

namespace ne {
    class PipelineManager;
    class GpuResource;
    class Pipeline;
    class DirectXResource;

    /**
     * Used for binding GPU resources as "global" HLSL shader resources (that don't change on a per-object
     * basis).
     */
    class HlslGlobalShaderResourceBinding : public GlobalShaderResourceBinding {
        // Only base class is allowed to create such objects.
        friend class GlobalShaderResourceBinding;

    public:
        HlslGlobalShaderResourceBinding() = delete;

        virtual ~HlslGlobalShaderResourceBinding() override;

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
        HlslGlobalShaderResourceBinding(
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

    private:
        /**
         * Specify a callback that will be called for each graphics pipeline to bind your resource.
         *
         * @param pPipelineManager Manager to query graphics pipelines.
         * @param onBind           Called on each pipeline.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] static std::optional<Error> bindResourceToGraphicsPipelines(
            PipelineManager* pPipelineManager,
            const std::function<std::optional<Error>(Pipeline* pPipeline)>& onBind);

        /**
         * Binds the specified resources to the specified pipeline as a global SRV binding.
         *
         * @param vResourcesToBind    Resource per frame to bind.
         * @param bindingType         Type of the binding to create.
         * @param pPipeline           Pipeline to bind to.
         * @param sShaderResourceName Name of the shader resource to bind to.
         *
         * @return
         */
        [[nodiscard]] static std::optional<Error> bindResourcesToPipeline(
            const std::array<DirectXResource*, FrameResourceManager::getFrameResourceCount()>&
                vResourcesToBind,
            DirectXDescriptorType bindingType,
            Pipeline* pPipeline,
            const std::string& sShaderResourceName);
    };
}
