#pragma once

// Standard.
#include <mutex>
#include <optional>
#include <unordered_set>

// Custom.
#include "render/general/resource/frame/FrameResourceManager.h"
#include "misc/Error.h"

namespace ne {
    class GpuResource;
    class Pipeline;
    class GlobalShaderResourceBinding;

    /**
     * Manages bindings of GPU resources as global shader resources (that don't change on a per-object
     * basis).
     */
    class GlobalShaderResourceBindingManager {
        // Only renderer should create this manager.
        friend class Renderer;

        // Pipeline manager will notify this object about pipelines being re-created.
        friend class PipelineManager;

        // Bindings will notify the manager upon construction and destruction.
        friend class GlobalShaderResourceBinding;

    public:
        GlobalShaderResourceBindingManager() = delete;

        /**
         * Creates a new global shader resource binding (that don't change on a per-object
         * basis) and assigns it to the specified resources. When resources will be destroyed
         * the binding will also be removed.
         *
         * @remark If you only need to bind the same GPU resource for all frames in-flight then either
         * use @ref createGlobalShaderResourceBindingSingleResource that accepts a single GPU resource
         * pointer (preferred) or just pass an array of the same pointers.
         *
         * @remark The actual type of the binding (cbuffer, uniform, structured buffer, storage buffer or
         * etc) will be determined from the resource. For example, in DirectX in order to bind a `cbuffer`
         * shader resource you need to pass a resource that already has a CBV binded and in Vulkan in
         * order to bind a `uniform` you need to make sure that your resource was created with
         * the "uniform" hint/flag.
         *
         * @param sShaderResourceName Name of the shader resource (name from shader code) to bind the
         * resources.
         * @param vResourcesToBind    Resources to bind to pipelines. This function will create a binding
         * that binds a separate GPU resource per frame, this is generally used for some CPU-write resources
         * so in this case you have a CPU-write GPU buffer that you plan to update without CPU-GPU
         * synchronization (for example each time the CPU is submitting a new frame) then you need to pass a
         * separate buffer per frame resource to avoid modifying a buffer that may be used by the GPU.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createGlobalShaderResourceBindingResourcePerFrame(
            const std::string& sShaderResourceName,
            std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()> vResourcesToBind);

        /**
         * Creates a new global shader resource binding (that don't change on a per-object
         * basis) and assigns it to the specified resources. When resources will be destroyed
         * the binding will also be removed.
         *
         * @remark Also see @ref createGlobalShaderResourceBindingResourcePerFrame for important remarks.
         *
         * @param sShaderResourceName Name of the shader resource (name from shader code) to bind the
         * resource.
         * @param pResourceToBind    Resources to bind to pipelines. This function will create a binding
         * that binds the same GPU resource for all frames in-flight (this can be used for textures
         * or some buffer resources). This is used when you guarantee the CPU-GPU synchronization or
         * don't plan to update the resource's contents from the CPU.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> createGlobalShaderResourceBindingSingleResource(
            const std::string& sShaderResourceName, GpuResource* pResourceToBind);

        /** Makes sure no active binding exists. */
        ~GlobalShaderResourceBindingManager();

    private:
        /**
         * Initializes a new manager.
         *
         * @param pPipelineManager Manager used to interact with pipelines.
         */
        GlobalShaderResourceBindingManager(PipelineManager* pPipelineManager);

        /**
         * Called by pipeline manager when a new graphics pipeline is created to
         * maybe bind some global shader resources to it.
         *
         * @param pNewPipeline Pipeline to bind to.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> onNewGraphicsPipelineCreated(Pipeline* pNewPipeline);

        /**
         * Called by pipeline manager after all graphics pipelines re-created their internal state
         * to re-bind all global shader resources to all pipelines.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> onAllGraphicsPipelinesRecreatedInternalResources();

        /**
         * Should be called by bindings upon construction.
         *
         * Registers the binding and binds it to all graphics pipelines that might need that binding.
         *
         * @param pBinding Binding to register.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> registerNewBinding(GlobalShaderResourceBinding* pBinding);

        /**
         * Should be called by bindings in their destructor.
         *
         * Unregisters a binding that was previously registered using @ref registerNewBinding.
         *
         * @param pBinding Binding to unregister.
         */
        void unregisterBinding(GlobalShaderResourceBinding* pBinding);

        /**
         * Set of all currently existing (active) bindings.
         *
         * @remark It's safe to store a raw pointer here because bindings will notify the manager in their
         * destructor.
         */
        std::pair<std::recursive_mutex, std::unordered_set<GlobalShaderResourceBinding*>> mtxActiveBindings;

        /** Manager used to interact with pipelines. */
        PipelineManager* const pPipelineManager = nullptr;
    };
}
