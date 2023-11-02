#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>
#include <functional>

// Custom.
#include "misc/Error.h"
#include "render/general/pipeline/PipelineManager.h"

namespace ne {
    class Renderer;

    /** Interface to configure and run a compute shader. */
    class ComputeShaderInterface {
    public:
        ComputeShaderInterface() = delete;
        virtual ~ComputeShaderInterface();

        ComputeShaderInterface(const ComputeShaderInterface&) = delete;
        ComputeShaderInterface& operator=(const ComputeShaderInterface&) = delete;

        /**
         * Creates a new render-specific interface to a compute shader that will be run in the graphics queue
         * (the same queue that the rendering uses), this compute shader will run synchronously
         * to the rendering operations (on the GPU).
         *
         * @remark You might want to run your compute shader in the graphics queue if you
         * need to make sure that some rendering operation(s) are run strictly before or after the compute
         * shader (for example if you need to calculate some data that the rendering will use then
         * use the graphics queue).
         *
         * @param pRenderer                  Used renderer.
         * @param sCompiledComputeShaderName Name of the compiled compute shader to later run.
         * @param bRunBeforeFrameRendering   Specify `true` if you want your compute shader to be run
         * strictly before the frame is rendered on the GPU (this can be useful if your compute shader
         * calculates some data that the frame rendering will use). Specify `false` if you want your
         * compute shader to be run strictly after the frame is rendered (this can be useful if your
         * compute shader does some post-processing for the rendered frame).
         *
         * @return Error if something went wrong, otherwise created interface.
         */
        static std::variant<std::unique_ptr<ComputeShaderInterface>, Error> createUsingGraphicsQueue(
            Renderer* pRenderer,
            const std::string& sCompiledComputeShaderName,
            bool bRunBeforeFrameRendering);

        /**
         * Adds this compute shader to the GPU queue to be executed.
         *
         * @remark This function queues the shader to be executed only once, if you want to
         * queue this shader again you need to call this function later (if using graphics queue -
         * after a new frame is drawn, otherwise later when you need it). Calling this function again
         * right after calling it the first time will do nothing if graphics queue is used but override the
         * specified thread group counts, if compute queue is used it will queue it again (so it will be
         * executed twice).
         *
         * @param iThreadGroupCountX The number of thread groups dispatched in the X direction.
         * @param iThreadGroupCountY The number of thread groups dispatched in the Y direction.
         * @param iThreadGroupCountZ The number of thread groups dispatched in the Z direction.
         */
        void submitForExecution(
            unsigned int iThreadGroupCountX,
            unsigned int iThreadGroupCountY,
            unsigned int iThreadGroupCountZ);

        /**
         * Returns name of the compiled compute shader that this interface uses.
         *
         * @return Compute shader name.
         */
        std::string getComputeShaderName() const;

        /**
         * Returns compute pipeline that this shader is referencing.
         *
         * @return Compute pipeline (always valid pointer unless this object is being destroyed).
         */
        Pipeline* getUsedPipeline() const;

    protected:
        /**
         * Creates a new fully initialized render-specific compute shader interface.
         *
         * @param pRenderer                Used renderer.
         * @param sComputeShaderName       Name of the compiled compute shader to use.
         * @param bRunBeforeFrameRendering Determines whether this compute shader should run before frame
         * rendering or after a frame is rendered on the GPU. Only valid when using graphics queue.
         *
         * @return Error if something went wrong, otherwise created interface.
         */
        static std::variant<std::unique_ptr<ComputeShaderInterface>, Error> createRenderSpecificInterface(
            Renderer* pRenderer, const std::string& sComputeShaderName, bool bRunBeforeFrameRendering = true);

        /**
         * Creates a new interface and initializes everything except for @ref pPipeline which is expected to
         * be initialized right after object creation.
         *
         * @param pRenderer                Used renderer.
         * @param sComputeShaderName       Name of the compiled compute shader to use.
         * @param bRunBeforeFrameRendering Determines whether this compute shader should run before frame
         * rendering or after a frame is rendered on the GPU. Only valid when using graphics queue.
         *
         * @return Created interface.
         */
        static std::unique_ptr<ComputeShaderInterface> createPartiallyInitializedRenderSpecificInterface(
            Renderer* pRenderer, const std::string& sComputeShaderName, bool bRunBeforeFrameRendering = true);

        /**
         * Initializes everything except for @ref pPipeline which is expected to be initialized right after
         * object creation.
         *
         * @param pRenderer                Used renderer.
         * @param sComputeShaderName       Name of the compiled compute shader to use.
         * @param bRunBeforeFrameRendering Determines whether this compute shader should run before frame
         * rendering or after a frame is rendered on the GPU. Only valid when using graphics queue.
         */
        ComputeShaderInterface(
            Renderer* pRenderer, const std::string& sComputeShaderName, bool bRunBeforeFrameRendering = true);

        /**
         * Returns used renderer.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Non-owning pointer to the renderer.
         */
        Renderer* getRenderer();

        /**
         * Returns used compute pipeline.
         *
         * @return Pipeline.
         */
        Pipeline* getPipeline() const;

        /**
         * Returns the number of thread groups dispatched in the X direction.
         *
         * @return Group count.
         */
        inline unsigned int getThreadGroupCountX() const { return iThreadGroupCountX; }

        /**
         * Returns the number of thread groups dispatched in the Y direction.
         *
         * @return Group count.
         */
        inline unsigned int getThreadGroupCountY() const { return iThreadGroupCountY; }

        /**
         * Returns the number of thread groups dispatched in the Z direction.
         *
         * @return Group count.
         */
        inline unsigned int getThreadGroupCountZ() const { return iThreadGroupCountZ; }

    private:
        /** Do not delete (free) this pointer. A non-owning pointer to the renderer. */
        Renderer* pRenderer = nullptr;

        /** Compute pipeline that this interface is using. */
        PipelineSharedPtr pPipeline;

        /** The number of thread groups dispatched in the X direction. */
        unsigned int iThreadGroupCountX = 0;

        /** The number of thread groups dispatched in the Y direction. */
        unsigned int iThreadGroupCountY = 0;

        /** The number of thread groups dispatched in the Z direction. */
        unsigned int iThreadGroupCountZ = 0;

        /**
         * Determines whether this compute shader should run before frame rendering or after a frame is
         * rendered on the GPU.
         *
         * @remark Only valid when using graphics queue.
         */
        const bool bRunBeforeFrameRendering = true;

        /** Name of the compiled compute shader to run. */
        const std::string sComputeShaderName;
    };
}
