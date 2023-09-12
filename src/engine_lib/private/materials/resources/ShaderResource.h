#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>
#include <functional>
#include <mutex>
#include <optional>

// Custom.
#include "materials/TextureManager.h"
#include "misc/Error.h"

namespace ne {
    class Pipeline;
    class GpuResource;
    class UploadBuffer;

    /**
     * Base class for shader resources.
     *
     * @remark A shader resource acts as a bridge between game/engine entities that want to set/bind
     * some data (like buffer/texture) to some shader resource (defined in HLSL/GLSL) and the renderer
     * that is able to set/bind the specified data to a binding that corresponds to the specified
     * shader resources (defined in HLSL/GLSL) so that the specified data can be accessed in shaders.
     */
    class ShaderResourceBase {
        // Only pipeline manager is expected to call `onAfterAllPipelinesRefreshedResources`.
        friend class PipelineManager;

    public:
        virtual ~ShaderResourceBase() = default;

        /**
         * Called to make the resource use some other pipeline.
         *
         * @warning Expects that the called is using some mutex to protect this shader resource
         * from being used in the `draw` function while this function is not finished
         * (i.e. make sure the CPU will not queue a new frame while this function is not finished).
         *
         * @remark For example, this function can be called from a mesh node on a shader resource
         * that references mesh's constant shader data (that stores mesh's world matrix and etc)
         * after spawned mesh node changed its material (and thus most likely the PSO too).
         *
         * @remark Shader resource now needs to check that everything that it needs
         * is still there and possibly re-bind to pipeline's descriptors since these might have
         * been also re-created.
         *
         * @param pNewPipeline New pipeline object that is now being used instead of the old one.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> useNewPipeline(Pipeline* pNewPipeline);

        /**
         * Returns name of this resource.
         *
         * @return Resource name.
         */
        std::string getResourceName() const;

    protected:
        /**
         * Initializes the resource.
         *
         * @param sResourceName Name of the resource we are referencing (should be exactly the same as
         * the resource name written in the shader file we are referencing).
         * @param pUsedPipeline Pipeline that this shader resource is using.
         */
        ShaderResourceBase(const std::string& sResourceName, Pipeline* pUsedPipeline);

        /**
         * Called from pipeline manager to notify that all pipelines released their internal resources
         * and now restored them so their internal resources (for example push constants) might
         * be different now and shader resource now needs to check that everything that it needs
         * is still there and possibly re-bind to pipeline's descriptors since these might have
         * been also re-created.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> onAfterAllPipelinesRefreshedResources();

        /**
         * Requires shader resource to fully (re)bind to a new/changed pipeline.
         *
         * @warning This function should not be called from outside of the class, it's only used for
         * derived implementations (called from @ref useNewPipeline). Instead use
         * @ref onAfterAllPipelinesRefreshedResources or @ref useNewPipeline.
         *
         * @remark Derived shader resources must refresh all of their variables received from
         * a previous pipeline because the specified (possibly new) pipeline might be completely
         * different.
         *
         * @remark This function is generally called when pipeline that shader resource references is
         * changed or when render settings were changed and internal resources of all pipelines
         * were re-created.
         *
         * @param pNewPipeline Pipeline to bind.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindToNewPipeline(Pipeline* pNewPipeline) = 0;

        /**
         * Returns currently used pipeline.
         *
         * @return Current pipeline.
         */
        std::pair<std::recursive_mutex, Pipeline*>* getCurrentPipeline();

    private:
        /** Current pipeline that this resource references. */
        std::pair<std::recursive_mutex, Pipeline*> mtxCurrentPipeline;

        /** Name of the resource we are referencing. */
        std::string sResourceName;
    };

    /**
     * References some bindless array/table from shader code and allows reserving a slot (space)
     * in this bindless array/table to be used for some custom-provided descriptors/views.
     */
    class ShaderBindlessTextureResource : public ShaderResourceBase {
    public:
        virtual ~ShaderBindlessTextureResource() override = default;

        /**
         * Makes the shader resource to reference the new (specified) texture.
         *
         * @warning Expects that the called is using some mutex to protect this shader resource
         * from being used in the `draw` function while this function is not finished
         * (i.e. make sure the CPU will not queue a new frame while this function is not finished).
         *
         * @param pTextureToUse Texture to reference.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> useNewTexture(std::unique_ptr<TextureHandle> pTextureToUse);

    protected:
        /**
         * Initializes the resource.
         *
         * @param sResourceName Name of the resource we are referencing (should be exactly the same as
         * the resource name written in the shader file we are referencing).
         * @param pUsedPipeline that this shader resource is using.
         */
        ShaderBindlessTextureResource(const std::string& sResourceName, Pipeline* pUsedPipeline);

        /**
         * Called to update the descriptor so that it will reference the new (specified) texture.
         *
         * @warning This function should not be called from outside of the class, it's only used for
         * derived implementations (called from @ref useNewTexture). Instead use @ref useNewTexture.
         *
         * @param pTextureToUse Texture to reference.
         * @param pUsedPipeline Pipeline that this shader resource is using.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        updateTextureDescriptor(std::unique_ptr<TextureHandle> pTextureToUse, Pipeline* pUsedPipeline) = 0;
    };

    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has CPU write access available (can be updated from the CPU side).
     */
    class ShaderCpuWriteResource : public ShaderResourceBase {
        // Only manager should be able to create and update resources of this type.
        friend class ShaderCpuWriteResourceManager;

    public:
        virtual ~ShaderCpuWriteResource() override;

        /**
         * Returns original size of the resource (not padded).
         *
         * @return Size in bytes.
         */
        inline size_t getOriginalResourceSizeInBytes() const { return iOriginalResourceSizeInBytes; }

    protected:
        /**
         * Constructs not fully initialized resource.
         *
         * @param sResourceName                Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param pUsedPipeline                Pipeline that this shader resource is using.
         * @param iOriginalResourceSizeInBytes Original size of the resource (not padded).
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         */
        ShaderCpuWriteResource(
            const std::string& sResourceName,
            Pipeline* pUsedPipeline,
            size_t iOriginalResourceSizeInBytes,
            const std::function<void*()>& onStartedUpdatingResource,
            const std::function<void()>& onFinishedUpdatingResource);

        /**
         * Function used to update resource data. Returns pointer to data of size
         * @ref iOriginalResourceSizeInBytes that needs to be copied into resource data storage (GPU
         * resource).
         */
        std::function<void*()> onStartedUpdatingResource;

        /** Function to call when finished updating (usually used for unlocking resource data mutex). */
        std::function<void()> onFinishedUpdatingResource;

        /** Original size of the resource (not padded). */
        size_t iOriginalResourceSizeInBytes = 0;
    };
} // namespace ne
