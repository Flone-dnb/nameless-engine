#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>
#include <functional>
#include <mutex>
#include <unordered_set>
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
         * Called to make the resource to use some other pipeline of a material.
         *
         * @warning Expects that the caller is using some mutex to protect this shader resource
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
         * @param pDeletedPipeline Old pipeline that was used and is probably deleted so don't dereference
         * or call member functions using this pointer. Only use it for things like `find` to replace
         * old pointer.
         * @param pNewPipeline     New pipeline to use instead of the old one.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        bindToChangedPipelineOfMaterial(Pipeline* pDeletedPipeline, Pipeline* pNewPipeline) = 0;

        /**
         * Called to make the resource to discard currently used pipelines and bind/reference
         * other pipelines.
         *
         * @warning Expects that the caller is using some mutex to protect this shader resource
         * from being used in the `draw` function while this function is not finished
         * (i.e. make sure the CPU will not queue a new frame while this function is not finished).
         *
         * @remark For example, for this function can be called from a mesh node that changed
         * its geometry and thus added/removed some material slots.
         *
         * @param pipelinesToUse Pipelines to use instead of the current ones.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        changeUsedPipelines(std::unordered_set<Pipeline*> pipelinesToUse) = 0;

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
         */
        ShaderResourceBase(const std::string& sResourceName);

        /**
         * Called from pipeline manager to notify that all pipelines released their internal resources
         * and now restored them so their internal resources (for example push constants) might
         * be different now and shader resource now needs to check that everything that it needs
         * is still there and possibly re-bind to pipeline's descriptors since these might have
         * been also re-created.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> onAfterAllPipelinesRefreshedResources() = 0;

    private:
        /** Name of the resource we are referencing. */
        std::string sResourceName;
    };

    /** References some texture (maybe bindless array/table) from shader code. */
    class ShaderTextureResource : public ShaderResourceBase {
    public:
        virtual ~ShaderTextureResource() override = default;

        /**
         * Makes the shader resource to reference the new (specified) texture.
         *
         * @warning Expects that the caller is using some mutex to protect this shader resource
         * from being used in the `draw` function while this function is not finished
         * (i.e. make sure the CPU will not queue a new frame while this function is not finished).
         *
         * @param pTextureToUse Texture to reference.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        useNewTexture(std::unique_ptr<TextureHandle> pTextureToUse) = 0;

    protected:
        /**
         * Initializes the resource.
         *
         * @param sResourceName Name of the resource we are referencing (should be exactly the same as
         * the resource name written in the shader file we are referencing).
         */
        ShaderTextureResource(const std::string& sResourceName);
    };

    /**
     * References a single (non-array) shader resource (that is written in a shader file)
     * that has CPU write access available (can be updated from the CPU side).
     */
    class ShaderCpuWriteResource : public ShaderResourceBase {
        // Only manager should be able to create and update resources of this type.
        friend class ShaderCpuWriteResourceManager;

    public:
        virtual ~ShaderCpuWriteResource() override = default;

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
         * @param iOriginalResourceSizeInBytes Original size of the resource (not padded).
         * @param onStartedUpdatingResource    Function that will be called when started updating resource
         * data. Function returns pointer to data of the specified resource data size that needs to be copied
         * into the resource.
         * @param onFinishedUpdatingResource   Function that will be called when finished updating
         * (usually used for unlocking resource data mutex).
         */
        ShaderCpuWriteResource(
            const std::string& sResourceName,
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
