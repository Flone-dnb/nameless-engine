#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>

// Custom.
#include "materials/resources/ShaderResource.h"
#include "materials/TextureManager.h"
#include "render/vulkan/pipeline/VulkanPushConstantsManager.hpp"
#include "materials/resources/ShaderBindlessArrayIndexManager.h"

// External.
#include "vulkan/vulkan.h"

namespace ne {
    class Pipeline;
    class VulkanPipeline;

    /**
     * References some bindless array/table from shader code and allows reserving a slot (space)
     * in this bindless array/table to be used for some custom-provided descriptors/views.
     */
    class GlslShaderBindlessTextureResource : public ShaderBindlessTextureResource {
        // Only shader resource manager should be able to create such resources.
        friend class ShaderBindlessTextureResourceManager;

    public:
        virtual ~GlslShaderBindlessTextureResource() override = default;

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
         */
        inline void copyResourceIndexToPushConstants(VulkanPushConstantsManager* pPushConstantsManager) {
            pPushConstantsManager->copyValueToPushConstant(
                iPushConstantIndex, pBindlessArrayIndex->getActualIndex());
        }

    protected:
        /**
         * Initializes the resource.
         *
         * @remark Used internally, for outside usage prefer to use @ref create.
         *
         * @param sResourceName        Name of the resource we are referencing (should be exactly
         * the same as the resource name written in the shader file we are referencing).
         * @param pUsedPipeline        Pipeline that uses the shader/parameters we are referencing.
         * @param pTextureToUse        Texture that should be binded to a descriptor in bindless
         * array.
         * @param pBindlessArrayIndex  Index into bindless array of textures that we copy to push
         * constants.
         * @param iPushConstantIndex   Index of push constant to copy texture index to.
         */
        GlslShaderBindlessTextureResource(
            const std::string& sResourceName,
            VulkanPipeline* pUsedPipeline,
            std::unique_ptr<TextureHandle> pTextureToUse,
            std::unique_ptr<BindlessArrayIndex> pBindlessArrayIndex,
            size_t iPushConstantIndex);

        /**
         * Requires shader resource to fully (re)bind to a new/changed pipeline.
         *
         * @warning This function should not be called from outside of the class, it's only used for
         * derived implementations.
         *
         * @remark This function is generally called when pipeline that shader resource references is
         * changed or when render settings were changed and internal resources of all pipelines
         * were re-created.
         *
         * @param pNewPipeline Pipeline to bind.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> bindToNewPipeline(Pipeline* pNewPipeline) override;

        /**
         * Called to update the descriptor so that it will reference the new (specified) texture.
         *
         * @warning This function should not be called from outside of the class, it's only used for
         * derived implementations.
         *
         * @param pTextureToUse Texture to reference.
         * @param pUsedPipeline Pipeline that this shader resource is using.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> updateTextureDescriptor(
            std::unique_ptr<TextureHandle> pTextureToUse, Pipeline* pUsedPipeline) override;

    private:
        /**
         * Creates a GLSL shader resource for referencing texture in bindless array.
         *
         * @param sShaderResourceName     Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param sResourceAdditionalInfo Additional text that we will append to created resource name
         * (used for logging).
         * @param pUsedPipeline           Pipeline that uses the shader/parameters we are referencing.
         * @param pTextureToUse           Texture that should be binded to a descriptor in bindless array.
         *
         * @return Error if something went wrong, otherwise created shader resource.
         */
        static std::variant<std::unique_ptr<ShaderBindlessTextureResource>, Error> create(
            const std::string& sShaderResourceName,
            const std::string& sResourceAdditionalInfo,
            Pipeline* pUsedPipeline,
            std::unique_ptr<TextureHandle> pTextureToUse);

        /**
         * Asks the index manager for an index into the requested bindless array resource.
         *
         * @param sShaderResourceName Name of the bindless array resource defined in GLSL.
         * @param pPipelineToLookIn   Pipeline to look for an index manager that will provide the index
         * into bindless array.
         *
         * @return Error if something went wrong, otherwise received index into bindless array.
         */
        static std::variant<std::unique_ptr<BindlessArrayIndex>, Error> getTextureIndexInBindlessArray(
            const std::string& sShaderResourceName, VulkanPipeline* pPipelineToLookIn);

        /**
         * Binds the specified image view to the sampler descriptor of the specified pipeline for
         * binding that corresponds to the specified shader resource name.
         *
         * @param sShaderResourceName      Name of the shader resource to get binding index in descriptor set.
         * @param pPipelineWithDescriptors Pipeline which descriptors to use.
         * @param pTextureView             Texture view to bind.
         * @param iIndexIntoBindlessArray  Index to a descriptor (in the bindless texture array) to bind.
         *
         * @return Error if something went wrong.
         */
        static std::optional<Error> bindTextureToBindlessDescriptorArray(
            const std::string& sShaderResourceName,
            VulkanPipeline* pPipelineWithDescriptors,
            VkImageView pTextureView,
            unsigned int iIndexIntoBindlessArray);

        /** Texture that we bind to descriptor. */
        std::pair<std::mutex, std::unique_ptr<TextureHandle>> mtxUsedTexture;

        /** Index into bindless array. */
        std::unique_ptr<BindlessArrayIndex> pBindlessArrayIndex;

        /** Index of push constant to copy @ref pBindlessArrayIndex to. */
        size_t iPushConstantIndex = 0;
    };
} // namespace ne
