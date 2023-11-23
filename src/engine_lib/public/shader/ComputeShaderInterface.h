#pragma once

// Standard.
#include <string>
#include <memory>
#include <vector>
#include <variant>
#include <functional>

// Custom.
#include "misc/Error.h"
#include "render/general/pipeline/PipelineSharedPtr.h"
#include "render/general/resources/GpuResource.h"

namespace ne {
    class Renderer;

    /** Describes usage of a resource in a compute shader. */
    enum class ComputeResourceUsage {
        READ_ONLY_ARRAY_BUFFER,  //< `StructuredBuffer` in HLSL, `readonly buffer` in GLSL.
        READ_WRITE_ARRAY_BUFFER, //< `RWStructuredBuffer` in HLSL, `buffer` in GLSL.
        CONSTANT_BUFFER,         //< `cbuffer` in HLSL, `uniform` in GLSL.
        READ_ONLY_TEXTURE,       //< `Texture2D` in HLSL, `uniform sampler2D` in GLSL.
        READ_WRITE_TEXTURE,      //< `RWTexture2D` in HLSL, `uniform image2D` in GLSL.
    };

    /** Describes when a compute shader should be executed. */
    enum class ComputeExecutionStage : size_t {
        AFTER_DEPTH_PREPASS = 0, //< After depth texture is fully written but before color rendering (main
                                 // pass) is < started.

        SIZE //< Marks the size of this enum.
    };

    /**
     * Splits compute shaders into groups where shaders of the first group will be executed before shaders
     * from the second group and so on.
     */
    enum class ComputeExecutionGroup : size_t {
        FIRST = 0,
        SECOND,
        SIZE //< Marks the size of this enum.
    };

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
         * @param executionStage             Determines when the shader will be executed.
         * @param executionGroup             Determines execution group of this shader where shaders of the
         * first group will be executed before shaders from the second group and so on.
         *
         * @return Error if something went wrong, otherwise created interface.
         */
        static std::variant<std::unique_ptr<ComputeShaderInterface>, Error> createUsingGraphicsQueue(
            Renderer* pRenderer,
            const std::string& sCompiledComputeShaderName,
            ComputeExecutionStage executionStage,
            ComputeExecutionGroup executionGroup = ComputeExecutionGroup::FIRST);

        /**
         * Takes ownership of the specified resource and binds it to be available in compute shader.
         *
         * @param pResource                  Resource to bind to compute shader.
         * @param sShaderResourceName        Resource name from shader.
         * @param usage                      Resource usage.
         * @param bUpdateOnlyCurrentFrameResourceDescriptors Specify `true` if you guarantee that you will
         * bind a different GPU resource on the next frame, specify `false` if you are not sure if you will
         * rebind the resource on the next frame or not. When `true` is specified only descriptors of the
         * current frame resource will be updated (because descriptors of other frame resources might be in
         * use and it's invalid to update them), when `false` descriptors of all frame resources will be
         * updated.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> bindResource(
            std::unique_ptr<GpuResource> pResource,
            const std::string& sShaderResourceName,
            ComputeResourceUsage usage,
            bool bUpdateOnlyCurrentFrameResourceDescriptors = false);

        /**
         * Binds the specified resource to be available in compute shaders.
         *
         * @warning This overload is used in cases where you cannot transfer resource ownership to the
         * compute shader interface. In this case you must guarantee that the resource will not be deleted
         * while this compute shader interface exists and while the GPU is processing this compute shader.
         *
         * @param pResource           Resource to bind to compute shader.
         * @param sShaderResourceName Resource name from shader.
         * @param usage               Resource usage.
         * @param bUpdateOnlyCurrentFrameResourceDescriptors Specify `true` if you guarantee that you will
         * bind a different GPU resource on the next frame, specify `false` if you are not sure if you will
         * rebind the resource on the next frame or not. When `true` is specified only descriptors of the
         * current frame resource will be updated (because descriptors of other frame resources might be in
         * use and it's invalid to update them), when `false` descriptors of all frame resources will be
         * updated.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindResource(
            GpuResource* pResource,
            const std::string& sShaderResourceName,
            ComputeResourceUsage usage,
            bool bUpdateOnlyCurrentFrameResourceDescriptors = false) = 0;

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
         * Returns execution group of this shader where shaders of the first group will be executed before
         * shaders from the second group and so on.
         *
         * @return Execution group.
         */
        ComputeExecutionGroup getExecutionGroup() const;

        /**
         * Returns execution stage of this shader.
         *
         * @return Execution stage.
         */
        ComputeExecutionStage getExecutionStage() const;

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
         * @param executionStage           Determines when the shader will be executed.
         * @param executionGroup           Determines execution group of this shader where shaders of the
         * first group will be executed before shaders from the second group and so on.
         *
         * @return Error if something went wrong, otherwise created interface.
         */
        static std::variant<std::unique_ptr<ComputeShaderInterface>, Error> createRenderSpecificInterface(
            Renderer* pRenderer,
            const std::string& sComputeShaderName,
            ComputeExecutionStage executionStage,
            ComputeExecutionGroup executionGroup);

        /**
         * Creates a new interface and initializes everything except for @ref pPipeline which is expected to
         * be initialized right after object creation.
         *
         * @param pRenderer                Used renderer.
         * @param sComputeShaderName       Name of the compiled compute shader to use.
         * @param executionStage           Determines when the shader will be executed.
         * @param executionGroup           Determines execution group of this shader where shaders of the
         * first group will be executed before shaders from the second group and so on.
         *
         * @return Created interface.
         */
        static std::unique_ptr<ComputeShaderInterface> createPartiallyInitializedRenderSpecificInterface(
            Renderer* pRenderer,
            const std::string& sComputeShaderName,
            ComputeExecutionStage executionStage,
            ComputeExecutionGroup executionGroup);

        /**
         * Initializes everything except for @ref pPipeline which is expected to be initialized right after
         * object creation.
         *
         * @param pRenderer                Used renderer.
         * @param sComputeShaderName       Name of the compiled compute shader to use.
         * @param executionStage           Determines when the shader will be executed.
         * @param executionGroup           Execution order.
         */
        ComputeShaderInterface(
            Renderer* pRenderer,
            const std::string& sComputeShaderName,
            ComputeExecutionStage executionStage,
            ComputeExecutionGroup executionGroup);

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
        /** Resources that this compute interface took ownership of. */
        std::vector<std::unique_ptr<GpuResource>> vOwnedResources;

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

        /** Describes when shader should be executed. */
        const ComputeExecutionStage executionStage = ComputeExecutionStage::AFTER_DEPTH_PREPASS;

        /** Describes order of execution. */
        const ComputeExecutionGroup executionGroup = ComputeExecutionGroup::FIRST;

        /** Name of the compiled compute shader to run. */
        const std::string sComputeShaderName;
    };
}
