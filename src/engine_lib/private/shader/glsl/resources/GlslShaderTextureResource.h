#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>
#include <unordered_set>

// Custom.
#include "shader/general/resources/ShaderResource.h"
#include "render/general/pipeline/PipelineShaderConstantsManager.hpp"
#include "shader/general/resources/ShaderArrayIndexManager.h"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    class TextureHandle;
    class Pipeline;
    class VulkanPipeline;

    /** References some texture from shader code. */
    class GlslShaderTextureResource : public ShaderTextureResource {
        // Only shader resource manager should be able to create such resources.
        friend class ShaderTextureResourceManager;

        /** Groups information about a specific push constant. */
        struct PushConstantIndices {
            /** Creates uninitialized object. */
            PushConstantIndices() = default;

            /**
             * Initializes object.
             *
             * @param iPushConstantIndex Index of the push constant to copy @ref pShaderArrayIndex.
             * @param pShaderArrayIndex  Index into shader array to copy to shaders.
             */
            PushConstantIndices(
                size_t iPushConstantIndex, std::unique_ptr<ShaderArrayIndex> pShaderArrayIndex) {
                this->iPushConstantIndex = iPushConstantIndex;
                this->pShaderArrayIndex = std::move(pShaderArrayIndex);
            }

            /** Move constructor. */
            PushConstantIndices(PushConstantIndices&&) = default;

            /**
             * Move assignment operator.
             *
             * @return this
             */
            PushConstantIndices& operator=(PushConstantIndices&&) = default;

            /** Index of the push constant to copy @ref pShaderArrayIndex. */
            size_t iPushConstantIndex = 0;

            /** Index into shader array to copy to shaders. */
            std::unique_ptr<ShaderArrayIndex> pShaderArrayIndex;
        };

    public:
        virtual ~GlslShaderTextureResource() override = default;

        /**
         * Returns path to a file/directory that stores used texture resource.
         *
         * @return Path relative to the `res` directory.
         */
        std::string getPathToTextureResource();

        /**
         * Copies resource index (into shader arrays) to a push constant.
         *
         * @param pPushConstantsManager Push constants manager.
         * @param pUsedPipeline         Current pipeline.
         */
        inline void copyResourceIndexToPushConstants(
            PipelineShaderConstantsManager* pPushConstantsManager, VulkanPipeline* pUsedPipeline) {
            // Since pipelines won't change here (because we are inside of the `draw` function)
            // we don't need to lock the mutex here.

            // Find push constant index of this pipeline.
            const auto it = mtxPushConstantIndices.second.find(pUsedPipeline);
            if (it == mtxPushConstantIndices.second.end()) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" was requested to set its push constant "
                    "index but this shader resource does not reference the specified pipeline",
                    getResourceName(),
                    mtxPushConstantIndices.second.size()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Copy value to push constants.
            pPushConstantsManager->copyValueToShaderConstant(
                it->second.iPushConstantIndex, it->second.pShaderArrayIndex->getActualIndex());
        }

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
        useNewTexture(std::unique_ptr<TextureHandle> pTextureToUse) override;

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
        changeUsedPipelines(const std::unordered_set<Pipeline*>& pipelinesToUse) override;

    protected:
        /**
         * Initializes the resource.
         *
         * @remark Used internally, for outside usage prefer to use @ref create.
         *
         * @param sResourceName        Name of the resource we are referencing (should be exactly
         * the same as the resource name written in the shader file we are referencing).
         * @param pTextureToUse        Texture that should be binded to a descriptor in shader
         * array.
         * @param pushConstantIndices Indices of push constants (per-pipeline) to copy texture index to.
         */
        GlslShaderTextureResource(
            const std::string& sResourceName,
            std::unique_ptr<TextureHandle> pTextureToUse,
            std::unordered_map<VulkanPipeline*, PushConstantIndices> pushConstantIndices);

        /**
         * Called from pipeline manager to notify that all pipelines released their internal resources
         * and now restored them so their internal resources (for example push constants) might
         * be different now and shader resource now needs to check that everything that it needs
         * is still there and possibly re-bind to pipeline's descriptors since these might have
         * been also re-created.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> onAfterAllPipelinesRefreshedResources() override;

    private:
        /**
         * Creates a GLSL shader resource for referencing texture in shader array.
         *
         * @param sShaderResourceName     Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param pipelinesToUse          Pipelines that use shader/parameters we are referencing.
         * @param pTextureToUse           Texture that should be binded to a descriptor in shader array.
         *
         * @return Error if something went wrong, otherwise created shader resource.
         */
        static std::variant<std::unique_ptr<ShaderTextureResource>, Error> create(
            const std::string& sShaderResourceName,
            const std::unordered_set<Pipeline*>& pipelinesToUse,
            std::unique_ptr<TextureHandle> pTextureToUse);

        /**
         * Asks the index manager for an index into the requested shader array resource.
         *
         * @param sShaderResourceName Name of the shader array resource defined in GLSL.
         * @param pPipelineToLookIn   Pipeline to look for an index manager that will provide the index
         * into shader array.
         *
         * @return Error if something went wrong, otherwise received index into shader array.
         */
        static std::variant<std::unique_ptr<ShaderArrayIndex>, Error> getTextureIndexInShaderArray(
            const std::string& sShaderResourceName, VulkanPipeline* pPipelineToLookIn);

        /**
         * Binds the specified image view to the sampler descriptor of the specified pipeline for
         * binding that corresponds to the specified shader resource name.
         *
         * @param sShaderResourceName      Name of the shader resource to get binding index in descriptor set.
         * @param pPipelineWithDescriptors Pipeline which descriptors to use.
         * @param pTextureView             Texture view to bind.
         * @param iIndexIntoShaderArray    Index to a descriptor (in the shader texture array) to bind.
         *
         * @return Error if something went wrong.
         */
        static std::optional<Error> bindTextureToShaderDescriptorArray(
            const std::string& sShaderResourceName,
            VulkanPipeline* pPipelineWithDescriptors,
            VkImageView pTextureView,
            unsigned int iIndexIntoShaderArray);

        /** Texture that we bind to descriptor. */
        std::pair<std::mutex, std::unique_ptr<TextureHandle>> mtxUsedTexture;

        /** Index of push constant (per-pipeline) to copy index into shader array. */
        std::pair<std::recursive_mutex, std::unordered_map<VulkanPipeline*, PushConstantIndices>>
            mtxPushConstantIndices;
    };
} // namespace ne
