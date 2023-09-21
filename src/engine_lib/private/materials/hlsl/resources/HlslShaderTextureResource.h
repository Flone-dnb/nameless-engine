#pragma once

// Standard.
#include <string>
#include <memory>
#include <unordered_set>
#include <variant>

// Custom.
#include "materials/resources/ShaderResource.h"
#include "render/directx/resources/DirectXResource.h"

namespace ne {
    class Pipeline;
    class DirectXPso;

    /** References some texture from shader code. */
    class HlslShaderTextureResource : public ShaderTextureResource {
        // Only shader resource manager should be able to create such resources.
        friend class ShaderTextureResourceManager;

    public:
        virtual ~HlslShaderTextureResource() override = default;

        /**
         * Adds a new command to the specified command list to use this shader resource.
         *
         * @warning Expects that this shader resource uses only 1 pipeline. Generally used for
         * material's resources because material can only reference 1 pipeline unlike mesh nodes.
         *
         * @param pCommandList Command list to add new command to.
         */
        inline void setGraphicsRootDescriptorTableOfOnlyPipeline(
            const ComPtr<ID3D12GraphicsCommandList>& pCommandList) const {
            // we don't need to lock texture mutex here because when this function is called it's called
            // inside of the `draw` function and shader resource mutex (outside) is locked which means:
            // - if our old texture resource is being destroyed before the GPU resource is deleted
            // the rendering will be stopped and thus we won't be in the `draw` function
            // - although after the old texture is destroyed we can start the `draw` function
            // the renderer will still need to lock shader resources mutex (in material for example)
            // but when the texture is being changed (in material for example) the material locks
            // shader resources mutex which means that when we get in this function the texture will
            // always be valid and so its GPU virtual address too

            // only descriptor index (offset) may change due to heap re-creation (expand/shrink)
            // but before a heap is destroyed (to be re-created) all rendering is stopped so
            // that no new frames will be queued until all descriptor offsets are updated

            const auto optionalDescriptorOffset = pTextureSrv->getDescriptorOffsetInDescriptors();

#if defined(DEBUG)
            // Make sure descriptor offset is still valid.
            if (!optionalDescriptorOffset.has_value()) [[unlikely]] {
                Error error(std::format(
                    "unable to get descriptor offset of SRV descriptor in shader resource \"{}\"",
                    getResourceName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Self check: make sure there is indeed just 1 pipeline.
            if (mtxRootParameterIndices.second.size() != 1) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" was requested to set its graphics root descriptor "
                    "table of the only used pipeline but this shader resource references "
                    "{} pipeline(s)",
                    getResourceName(),
                    mtxRootParameterIndices.second.size()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            // Set table.
            pCommandList->SetGraphicsRootDescriptorTable(
                mtxRootParameterIndices.second.begin()->second,
                D3D12_GPU_DESCRIPTOR_HANDLE{
                    iSrvHeapStart + (*optionalDescriptorOffset) * iSrvDescriptorSize});
        }

        /**
         * Adds a new command to the specified command list to use this shader resource.
         *
         * @param pCommandList  Command list to add new command to.
         * @param pUsedPipeline Current pipeline.
         */
        inline void setGraphicsRootDescriptorTableOfPipeline(
            const ComPtr<ID3D12GraphicsCommandList>& pCommandList, DirectXPso* pUsedPipeline) const {
            // same thing as above, we don't need to lock mutexes here

            const auto optionalDescriptorOffset = pTextureSrv->getDescriptorOffsetInDescriptors();

#if defined(DEBUG)
            // Make sure descriptor offset is still valid.
            if (!optionalDescriptorOffset.has_value()) [[unlikely]] {
                Error error(std::format(
                    "unable to get descriptor offset of SRV descriptor in shader resource \"{}\"",
                    getResourceName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            // Find root parameter index of this pipeline.
            const auto it = mtxRootParameterIndices.second.find(pUsedPipeline);
            if (it == mtxRootParameterIndices.second.end()) [[unlikely]] {
                Error error(std::format(
                    "shader resource \"{}\" was requested to set its graphics root descriptor table "
                    "but this shader resource does not reference the specified pipeline",
                    getResourceName(),
                    mtxRootParameterIndices.second.size()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }

            // Set table.
            pCommandList->SetGraphicsRootDescriptorTable(
                it->second,
                D3D12_GPU_DESCRIPTOR_HANDLE{
                    iSrvHeapStart + (*optionalDescriptorOffset) * iSrvDescriptorSize});
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
         * @param pTextureToUse        Texture to which a descriptor should be binded.
         * @param rootParameterIndices Indices of this resource in root signature.
         */
        HlslShaderTextureResource(
            const std::string& sResourceName,
            std::unique_ptr<TextureHandle> pTextureToUse,
            const std::unordered_map<DirectXPso*, UINT>& rootParameterIndices);

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
        static std::variant<std::unique_ptr<ShaderTextureResource>, Error> create(
            const std::string& sShaderResourceName,
            const std::unordered_set<Pipeline*>& pipelinesToUse,
            std::unique_ptr<TextureHandle> pTextureToUse);

        /** Texture to which a descriptor should be binded. */
        std::pair<std::mutex, std::unique_ptr<TextureHandle>> mtxUsedTexture;

        /** Descriptor binded to @ref mtxUsedTexture. */
        DirectXDescriptor* pTextureSrv = nullptr;

        /** GPU descriptor handle for SRV heap start. */
        UINT64 iSrvHeapStart = 0;

        /** Size of one SRV descriptor. */
        UINT iSrvDescriptorSize = 0;

        /** Indices of this resource in root signature to bind this resource during the draw operation. */
        std::pair<std::recursive_mutex, std::unordered_map<DirectXPso*, UINT>> mtxRootParameterIndices;
    };
} // namespace ne
