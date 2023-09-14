#pragma once

// Standard.
#include <string>
#include <memory>
#include <variant>

// Custom.
#include "materials/resources/ShaderResource.h"
#include "render/directx/resources/DirectXResource.h"

namespace ne {
    class Pipeline;

    /** References some texture from shader code. */
    class HlslShaderTextureResource : public ShaderTextureResource {
        // Only shader resource manager should be able to create such resources.
        friend class ShaderTextureResourceManager;

    public:
        virtual ~HlslShaderTextureResource() override = default;

        /**
         * Adds a new command to the specified command list to use this shader resource.
         *
         * @param pCommandList Command list to add new command to.
         */
        inline void setShaderResourceView(const ComPtr<ID3D12GraphicsCommandList>& pCommandList) const {
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
            if (!optionalDescriptorOffset.has_value()) [[unlikely]] {
                Error error(std::format(
                    "unable to get descriptor offset of SRV descriptor in shader resource \"{}\"",
                    getResourceName()));
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
#endif

            pCommandList->SetGraphicsRootDescriptorTable(
                iRootParameterIndex,
                D3D12_GPU_DESCRIPTOR_HANDLE{
                    iSrvHeapStart + (*optionalDescriptorOffset) * iSrvDescriptorSize});
        }

    protected:
        /**
         * Initializes the resource.
         *
         * @remark Used internally, for outside usage prefer to use @ref create.
         *
         * @param sResourceName        Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param pUsedPipeline        Pipeline that this resource references.
         * @param pTextureToUse        Texture to which a descriptor should be binded.
         * @param iRootParameterIndex  Index of this resource in root signature.
         */
        HlslShaderTextureResource(
            const std::string& sResourceName,
            Pipeline* pUsedPipeline,
            std::unique_ptr<TextureHandle> pTextureToUse,
            UINT iRootParameterIndex);

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
         * Creates a new HLSL shader resource.
         *
         * @param sShaderResourceName  Name of the resource we are referencing (should be exactly the
         * same as the resource name written in the shader file we are referencing).
         * @param pUsedPipeline        Pipeline that this resource references.
         * @param pTextureToUse        Texture to which a descriptor should be binded.
         *
         * @return Error if something went wrong, otherwise created shader resource.
         */
        static std::variant<std::unique_ptr<ShaderTextureResource>, Error> create(
            const std::string& sShaderResourceName,
            Pipeline* pUsedPipeline,
            std::unique_ptr<TextureHandle> pTextureToUse);

        /** Texture to which a descriptor should be binded. */
        std::pair<std::mutex, std::unique_ptr<TextureHandle>> mtxUsedTexture;

        /** Descriptor binded to @ref mtxUsedTexture. */
        DirectXDescriptor* pTextureSrv = nullptr;

        /** GPU descriptor handle for SRV heap start. */
        UINT64 iSrvHeapStart = 0;

        /** Size of one SRV descriptor. */
        UINT iSrvDescriptorSize = 0;

        /** Index of this resource in root signature to bind this resource during the draw operation. */
        UINT iRootParameterIndex = 0;
    };
} // namespace ne
