#pragma once

// Standard.
#include <string>
#include <memory>
#include <unordered_set>
#include <variant>
#include <mutex>

// Custom.
#include "shader/general/resource/binding/ShaderResourceBinding.h"
#include "render/directx/descriptors/DirectXDescriptorHeap.h"
#include "render/general/pipeline/PipelineShaderConstantsManager.hpp"
#include "render/directx/resource/DirectXResource.h"

namespace ne {
    class TextureHandle;
    class Pipeline;
    class DirectXPso;

    /** References some texture from shader code. */
    class HlslShaderTextureResourceBinding : public ShaderTextureResourceBinding {
        // Only the manager should be able to create such resources.
        friend class ShaderTextureResourceBindingManager;

    public:
        virtual ~HlslShaderTextureResourceBinding() override = default;

        /**
         * Copies resource index (into shader arrays) to a root constant.
         *
         * @param pShaderConstantsManager Shader constants manager.
         * @param pUsedPipeline           Current pipeline.
         */
        inline void copyResourceIndexToRootConstants(
            PipelineShaderConstantsManager* pShaderConstantsManager, DirectXPso* pUsedPipeline) {
            // Since pipelines won't change here (because we are inside of the `draw` function)
            // we don't need to lock the mutex here.

            // Find shader constant offset.
            const auto it = mtxShaderConstantOffsetPerPipeline.second.find(pUsedPipeline);
            if (it == mtxShaderConstantOffsetPerPipeline.second.end()) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" was requested to set its root constant "
                    "index but this shader resource does not reference the specified pipeline",
                    getShaderResourceName(),
                    mtxShaderConstantOffsetPerPipeline.second.size()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
            const auto& iShaderConstantIndex = mtxShaderConstantOffsetPerPipeline.second.begin()->second;

            // Query texture's SRV descriptor offset in the descriptor range.
            unsigned int iRootConstantValue = 0;
            {
                std::scoped_lock textureGuard(mtxUsedTexture.first);

                const auto pDirectXTexture =
                    reinterpret_cast<DirectXResource*>(mtxUsedTexture.second->getResource());

                // Get SRV.
                const auto pSrvDescriptor = pDirectXTexture->getDescriptor(DirectXDescriptorType::SRV);
                if (pSrvDescriptor == nullptr) [[unlikely]] {
                    Error error(std::format(
                        "shader resource \"{}\" was requested to set its root constant "
                        "index but used texture \"{}\" does not seem to have an SRV bound",
                        getShaderResourceName(),
                        pDirectXTexture->getResourceName()));
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }

                // Calculate descriptor offset. This may not be as fast as we want but this is the price we
                // pay for having a simple approach. We could have cached the offset but we would need to keep
                // the cached offset updated after the range resizes which seems like a complicated thing.
                auto result = pSrvDescriptor->getDescriptorOffsetFromRangeStart();
                if (std::holds_alternative<Error>(result)) [[unlikely]] {
                    auto error = std::get<Error>(std::move(result));
                    error.addCurrentLocationToErrorStack();
                    error.showError();
                    throw std::runtime_error(error.getFullErrorMessage());
                }

                iRootConstantValue = std::get<unsigned int>(result);
            }

            // Copy value to root constants.
            pShaderConstantsManager->copyValueToShaderConstant(iShaderConstantIndex, iRootConstantValue);
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
         * @param sResourceName        Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param pTextureToUse        Texture with an SRV already bound from an SRV range.
         * @param shaderConstantOffsetPerPipeline A shader constant index for our shader resource (per
         * pipeline).
         */
        HlslShaderTextureResourceBinding(
            const std::string& sResourceName,
            std::unique_ptr<TextureHandle> pTextureToUse,
            std::unordered_map<DirectXPso*, size_t>&& shaderConstantOffsetPerPipeline);

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
         * Creates a new HLSL shader resource.
         *
         * @param sShaderResourceName  Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param pipelinesToUse       Pipelines that use shader/parameters we are referencing.
         * @param pTextureToUse        Texture to which a descriptor should be binded.
         *
         * @return Error if something went wrong, otherwise created shader resource.
         */
        static std::variant<std::unique_ptr<ShaderTextureResourceBinding>, Error> create(
            const std::string& sShaderResourceName,
            const std::unordered_set<Pipeline*>& pipelinesToUse,
            std::unique_ptr<TextureHandle> pTextureToUse);

        /**
         * In the specified pipeline looks for a descriptor range that handles a shader resource
         * with the specified name (creates a new descriptor range if not found and returns a pointer to it).
         *
         * @param pPipeline           Pipeline to look in.
         * @param sShaderResourceName Shader resource to look for.
         *
         * @return Error if something went wrong, otherwise a pointer to descriptor range from the pipeline.
         */
        static std::variant<std::shared_ptr<ContinuousDirectXDescriptorRange>, Error>
        getSrvDescriptorRange(DirectXPso* pPipeline, const std::string& sShaderResourceName);

        /** Texture to which a descriptor should be bound. */
        std::pair<std::mutex, std::unique_ptr<TextureHandle>> mtxUsedTexture;

        /** Stores an offset of the shader constant to copy @ref mtxUsedTexture SRV descriptor offset to. */
        std::pair<std::recursive_mutex, std::unordered_map<DirectXPso*, size_t>>
            mtxShaderConstantOffsetPerPipeline;

        /**
         * `false` to avoid binding descriptors to cubemap faces - I don't see any point
         * in that here, we don't use individual cubemap faces in this case
         */
        static constexpr bool bBindSrvToCubemapFaces = false;
    };
} // namespace ne
