#pragma once

// Standard.
#include <string>
#include <memory>
#include <unordered_set>
#include <optional>

// Custom.
#include "material/TextureManager.h"
#include "misc/Error.h"

namespace ne {
    class Pipeline;
    class GpuResource;
    class UploadBuffer;
    class ShaderCpuWriteResourceManager;

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
         * Called to make the resource to discard currently used pipelines and bind/reference
         * other pipelines.
         *
         * @warning Expects that the caller is using some mutex to protect this shader resource
         * from being used in the `draw` function while this function is not finished
         * (i.e. make sure the CPU will not queue a new frame while this function is not finished).
         *
         * @remark For example, for this function can be called from a mesh node that changed
         * its geometry and thus added/removed some material slots, or if some material that mesh node
         * is using changed its pipeline.
         *
         * @param pipelinesToUse Pipelines to use instead of the current ones.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error>
        changeUsedPipelines(const std::unordered_set<Pipeline*>& pipelinesToUse) = 0;

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

    /** References some texture (maybe array/table) from shader code. */
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
} // namespace ne
