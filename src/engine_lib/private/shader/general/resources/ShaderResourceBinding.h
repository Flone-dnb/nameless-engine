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
    class ShaderCpuWriteResourceBindingManager;

    /**
     * Base class for shader resource bindings.
     *
     * @remark A shader resource binding acts as a bridge between game/engine entities that want to set/bind
     * some data (like buffer/texture) to some shader resource (defined in HLSL/GLSL) and the renderer
     * that is able to set/bind the specified data to a binding that corresponds to the specified
     * shader resources (defined in HLSL/GLSL) so that the specified data can be accessed in shaders.
     */
    class ShaderResourceBindingBase {
        // Only pipeline manager is expected to call `onAfterAllPipelinesRefreshedResources`.
        friend class PipelineManager;

    public:
        virtual ~ShaderResourceBindingBase() = default;

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
         * Returns name of the resource (from the shader code) that this binding references.
         *
         * @return Resource name.
         */
        std::string getShaderResourceName() const;

    protected:
        /**
         * Initializes the binding.
         *
         * @param sShaderResourceName Name of the resource we are referencing (should be exactly the same as
         * the resource name written in the shader file we are referencing).
         */
        ShaderResourceBindingBase(const std::string& sShaderResourceName);

        /**
         * Called from the pipeline manager to notify that all pipelines released their internal resources
         * and now restored them so their internal resources (for example push constants) might
         * be different now and shader resource now needs to check that everything that it needs
         * is still there and possibly re-bind to pipeline's descriptors since these might have
         * been also re-created.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> onAfterAllPipelinesRefreshedResources() = 0;

    private:
        /** Name of the resource we are referencing (name in the shader code). */
        const std::string sShaderResourceName;
    };

    /** References some texture from shader code (can also a single texture in the array of textures). */
    class ShaderTextureResourceBinding : public ShaderResourceBindingBase {
    public:
        virtual ~ShaderTextureResourceBinding() override = default;

        /**
         * Makes the binding to reference the new (specified) texture.
         *
         * @warning Expects that the caller is using some mutex to protect this binding
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
         * Initializes the binding.
         *
         * @param sShaderResourceName Name of the resource we are referencing (should be exactly the same as
         * the resource name written in the shader file we are referencing).
         */
        ShaderTextureResourceBinding(const std::string& sShaderResourceName);
    };
} // namespace ne
