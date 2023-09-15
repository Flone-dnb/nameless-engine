#include "GlslShaderTextureResource.h"

// Standard.
#include <format>

// Custom.
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "materials/glsl/resources/GlslShaderResourceHelpers.h"
#include "materials/DescriptorConstants.hpp"

namespace ne {

    std::variant<std::unique_ptr<ShaderTextureResource>, Error> GlslShaderTextureResource::create(
        const std::string& sShaderResourceName,
        Pipeline* pUsedPipeline,
        std::unique_ptr<TextureHandle> pTextureToUse) {
        // Convert pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pUsedPipeline);
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        // Find a resource with the specified name in the descriptor set layout and get push constant
        // index to use.
        auto pushConstantResult =
            GlslShaderResourceHelpers::getPushConstantIndex(pVulkanPipeline, sShaderResourceName);
        if (std::holds_alternative<Error>(pushConstantResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(pushConstantResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        const auto iPushConstantIndex = std::get<size_t>(pushConstantResult);

        // Get an index into the bindless array.
        auto bindlessArrayIndexResult = getTextureIndexInBindlessArray(sShaderResourceName, pVulkanPipeline);
        if (std::holds_alternative<Error>(bindlessArrayIndexResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(bindlessArrayIndexResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        auto pBindlessArrayIndex =
            std::get<std::unique_ptr<BindlessArrayIndex>>(std::move(bindlessArrayIndexResult));

        // Get texture image view.
        const auto pTextureResource = dynamic_cast<VulkanResource*>(pTextureToUse->getResource());
        if (pTextureResource == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource");
        }
        const auto pImageView = pTextureResource->getInternalImageView();
        if (pImageView == nullptr) [[unlikely]] {
            return Error("expected the texture's image view to be valid");
        }

        // Bind image to descriptor.
        auto optionalError = bindTextureToBindlessDescriptorArray(
            sShaderResourceName, pVulkanPipeline, pImageView, pBindlessArrayIndex->getActualIndex());
        if (optionalError.has_value()) [[unlikely]] {
            auto error = std::move(optionalError.value());
            error.addCurrentLocationToErrorStack();
            return error;
        }

        return std::unique_ptr<GlslShaderTextureResource>(new GlslShaderTextureResource(
            sShaderResourceName,
            pVulkanPipeline,
            std::move(pTextureToUse),
            std::move(pBindlessArrayIndex),
            iPushConstantIndex));
    }

    std::variant<std::unique_ptr<BindlessArrayIndex>, Error>
    GlslShaderTextureResource::getTextureIndexInBindlessArray(
        const std::string& sShaderResourceName, VulkanPipeline* pPipelineToLookIn) {
        // Get pipeline's internal resources.
        const auto pMtxPipelineResources = pPipelineToLookIn->getInternalResources();
        std::scoped_lock guard(pMtxPipelineResources->first);

        // Get index managers.
        auto& indexManagers = pMtxPipelineResources->second.bindlessArrayIndexManagers;

        // See if an index manager responsible for the specified resource exists.
        auto it = indexManagers.find(sShaderResourceName);
        if (it == indexManagers.end()) {
            // Create a new index manager.
            auto [insertIt, bIsInserted] =
                indexManagers.insert(std::pair<std::string, std::unique_ptr<ShaderBindlessArrayIndexManager>>{
                    sShaderResourceName,
                    std::make_unique<ShaderBindlessArrayIndexManager>(
                        std::format(
                            "{} (pipeline \"{}\")",
                            sShaderResourceName,
                            pPipelineToLookIn->getPipelineIdentifier()),
                        DescriptorConstants::iBindlessTextureArrayDescriptorCount)});
            it = insertIt;
        }

        // Return new index.
        return it->second->getNewIndex();
    }

    std::optional<Error> GlslShaderTextureResource::bindTextureToBindlessDescriptorArray(
        const std::string& sShaderResourceName,
        VulkanPipeline* pPipelineWithDescriptors,
        VkImageView pTextureView,
        unsigned int iIndexIntoBindlessArray) {
        // Get pipeline's internal resources.
        const auto pMtxPipelineResources = pPipelineWithDescriptors->getInternalResources();
        std::scoped_lock guard(pMtxPipelineResources->first);

        // Find a shader resource binding using the specified name.
        const auto bindingIt = pMtxPipelineResources->second.resourceBindings.find(sShaderResourceName);
        if (bindingIt == pMtxPipelineResources->second.resourceBindings.end()) [[unlikely]] {
            return Error(std::format(
                "unable to find a shader resource by the specified name \"{}\" in pipeline \"{}\"",
                sShaderResourceName,
                pPipelineWithDescriptors->getPipelineIdentifier()));
        }
        const auto iBindingIndex = bindingIt->second;

        // Get renderer.
        const auto pRenderer = dynamic_cast<VulkanRenderer*>(pPipelineWithDescriptors->getRenderer());
        if (pRenderer == nullptr) [[unlikely]] {
            return Error(std::format("expected a Vulkan renderer"));
        }

        // Get logical device to be used.
        const auto pLogicalDevice = pRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("logical device is `nullptr`");
        }

        // Get texture sampler.
        const auto pTextureSampler = pRenderer->getTextureSampler();
        if (pTextureSampler == nullptr) [[unlikely]] {
            return Error("texture sampler is `nullptr`");
        }

        // Update one descriptor in set per frame resource.
        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            // Prepare info to bind an image view to descriptor.
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = pTextureView;
            imageInfo.sampler = pTextureSampler;

            // Bind reserved space to descriptor.
            VkWriteDescriptorSet descriptorUpdateInfo{};
            descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorUpdateInfo.dstSet =
                pMtxPipelineResources->second.vDescriptorSets[i]; // descriptor set to update
            descriptorUpdateInfo.dstBinding = iBindingIndex;      // descriptor binding index
            descriptorUpdateInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorUpdateInfo.descriptorCount = 1; // how much descriptors in array to update
            descriptorUpdateInfo.dstArrayElement =
                iIndexIntoBindlessArray;                  // first descriptor in array to update
            descriptorUpdateInfo.pImageInfo = &imageInfo; // descriptor refers to image data

            // Update descriptor.
            vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
        }

        return {};
    }

    std::optional<Error> GlslShaderTextureResource::bindToNewPipeline(Pipeline* pNewPipeline) {
        // Convert pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pNewPipeline);
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        // Find a resource with the specified name in the descriptor set layout and get push constant
        // index to use.
        auto pushConstantResult =
            GlslShaderResourceHelpers::getPushConstantIndex(pVulkanPipeline, getResourceName());
        if (std::holds_alternative<Error>(pushConstantResult)) [[unlikely]] {
            auto error = std::get<Error>(std::move(pushConstantResult));
            error.addCurrentLocationToErrorStack();
            return error;
        }
        iPushConstantIndex = std::get<size_t>(pushConstantResult);

        std::scoped_lock guard(mtxUsedTexture.first);

        // Get texture image view.
        const auto pTextureResource = dynamic_cast<VulkanResource*>(mtxUsedTexture.second->getResource());
        if (pTextureResource == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource");
        }
        const auto pImageView = pTextureResource->getInternalImageView();
        if (pImageView == nullptr) [[unlikely]] {
            return Error("expected the texture's image view to be valid");
        }

        // Re-bind descriptors because they were re-created.
        auto optionalError = bindTextureToBindlessDescriptorArray(
            getResourceName(), pVulkanPipeline, pImageView, pBindlessArrayIndex->getActualIndex());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::optional<Error> GlslShaderTextureResource::updateTextureDescriptor(
        std::unique_ptr<TextureHandle> pTextureToUse, Pipeline* pUsedPipeline) {
        // Convert pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pUsedPipeline);
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        std::scoped_lock guard(mtxUsedTexture.first);

        // Replace used texture.
        mtxUsedTexture.second = std::move(pTextureToUse);

        // Get texture image view.
        const auto pTextureResource = dynamic_cast<VulkanResource*>(mtxUsedTexture.second->getResource());
        if (pTextureResource == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource");
        }
        const auto pImageView = pTextureResource->getInternalImageView();
        if (pImageView == nullptr) [[unlikely]] {
            return Error("expected the texture's image view to be valid");
        }

        // Re-bind descriptors because they were re-created.
        auto optionalError = bindTextureToBindlessDescriptorArray(
            getResourceName(), pVulkanPipeline, pImageView, pBindlessArrayIndex->getActualIndex());
        if (optionalError.has_value()) [[unlikely]] {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    std::string GlslShaderTextureResource::getPathToTextureResource() {
        std::scoped_lock guard(mtxUsedTexture.first);

        return mtxUsedTexture.second->getPathToResourceRelativeRes();
    }

    GlslShaderTextureResource::GlslShaderTextureResource(
        const std::string& sResourceName,
        VulkanPipeline* pUsedPipeline,
        std::unique_ptr<TextureHandle> pTextureToUse,
        std::unique_ptr<BindlessArrayIndex> pBindlessArrayIndex,
        size_t iPushConstantIndex)
        : ShaderTextureResource(sResourceName, pUsedPipeline) {
        this->pBindlessArrayIndex = std::move(pBindlessArrayIndex);
        this->iPushConstantIndex = iPushConstantIndex;

        mtxUsedTexture.second = std::move(pTextureToUse);
    }

} // namespace ne
