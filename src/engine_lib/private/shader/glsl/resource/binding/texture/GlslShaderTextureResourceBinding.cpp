#include "GlslShaderTextureResourceBinding.h"

// Standard.
#include <format>

// Custom.
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "shader/general/DescriptorConstants.hpp"
#include "render/general/pipeline/PipelineManager.h"
#include "material/TextureHandle.h"

namespace ne {

    std::variant<std::unique_ptr<ShaderTextureResourceBinding>, Error>
    GlslShaderTextureResourceBinding::create(
        const std::string& sShaderResourceName,
        const std::unordered_set<Pipeline*>& pipelinesToUse,
        std::unique_ptr<TextureHandle> pTextureToUse) {
        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }
        const auto pRenderer = (*pipelinesToUse.begin())->getRenderer();

        // Cast type.
        const auto pTextureResource = dynamic_cast<VulkanResource*>(pTextureToUse->getResource());
        if (pTextureResource == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource");
        }

        // Make sure no pipeline will re-create its internal resources because we will now reference
        // pipeline's internal resources. After we create a new shader resource binding object we can release
        // the mutex since shader resource bindings are notified after pipelines re-create their internal
        // resources.
        const auto pMtxGraphicsPipelines = pRenderer->getPipelineManager()->getGraphicsPipelines();
        std::scoped_lock pipelinesGuard(pMtxGraphicsPipelines->first);

        // Find push constant indices to use.
        std::unordered_map<VulkanPipeline*, PushConstantIndices> pushConstantIndices;
        for (const auto& pPipeline : pipelinesToUse) {
            // Find push constant.
            auto pushConstantResult = pPipeline->getUintConstantOffset(sShaderResourceName);
            if (std::holds_alternative<Error>(pushConstantResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(pushConstantResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto iPushConstantIndex = std::get<size_t>(pushConstantResult);

            // Convert pipeline.
            const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline);
            if (pVulkanPipeline == nullptr) [[unlikely]] {
                return Error("expected a Vulkan pipeline");
            }

            // Get an index into the shader array.
            auto shaderArrayIndexResult = getTextureIndexInShaderArray(sShaderResourceName, pVulkanPipeline);
            if (std::holds_alternative<Error>(shaderArrayIndexResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(shaderArrayIndexResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            auto pShaderArrayIndex =
                std::get<std::unique_ptr<ShaderArrayIndex>>(std::move(shaderArrayIndexResult));

            // Bind image to descriptor.
            auto optionalError = bindTextureToShaderDescriptorArray(
                sShaderResourceName, pVulkanPipeline, pTextureResource, pShaderArrayIndex->getActualIndex());
            if (optionalError.has_value()) [[unlikely]] {
                auto error = std::move(optionalError.value());
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Save a pair of "pipeline" - "index of push constant & array index".
            pushConstantIndices[pVulkanPipeline] =
                PushConstantIndices(iPushConstantIndex, std::move(pShaderArrayIndex));
        }

        // Pass data to the binding.
        auto pTextureResourceBinding =
            std::unique_ptr<GlslShaderTextureResourceBinding>(new GlslShaderTextureResourceBinding(
                sShaderResourceName, std::move(pTextureToUse), std::move(pushConstantIndices)));

        // At this point we can release the pipelines mutex.

        return pTextureResourceBinding;
    }

    std::variant<std::unique_ptr<ShaderArrayIndex>, Error>
    GlslShaderTextureResourceBinding::getTextureIndexInShaderArray(
        const std::string& sShaderResourceName, VulkanPipeline* pPipelineToLookIn) {
        // Get pipeline's internal resources.
        const auto pMtxPipelineResources = pPipelineToLookIn->getInternalResources();
        std::scoped_lock guard(pMtxPipelineResources->first);

        // Get index managers.
        auto& indexManagers = pMtxPipelineResources->second.shaderArrayIndexManagers;

        // See if an index manager responsible for the specified resource exists.
        auto it = indexManagers.find(sShaderResourceName);
        if (it == indexManagers.end()) {
            // Create a new index manager.
            auto [insertIt, bIsInserted] =
                indexManagers.insert(std::pair<std::string, std::unique_ptr<ShaderArrayIndexManager>>{
                    sShaderResourceName,
                    std::make_unique<ShaderArrayIndexManager>(
                        std::format(
                            "{} (pipeline \"{}\")",
                            sShaderResourceName,
                            pPipelineToLookIn->getPipelineIdentifier()),
                        DescriptorConstants::iBindlessTextureArrayDescriptorCount)});
            it = insertIt;
        }

        // Return new index.
        return it->second->reserveIndex();
    }

    std::optional<Error> GlslShaderTextureResourceBinding::bindTextureToShaderDescriptorArray(
        const std::string& sShaderResourceName,
        VulkanPipeline* pPipelineWithDescriptors,
        VulkanResource* pTexture,
        unsigned int iIndexIntoShaderArray) {
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
        const auto pTextureSampler = pTexture->getTextureSamplerForThisImage();

        // Update one descriptor in set per frame resource.
        for (unsigned int i = 0; i < FrameResourceManager::getFrameResourceCount(); i++) {
            // Prepare info to bind an image view to descriptor.
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = pTexture->getInternalImageView();
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
                iIndexIntoShaderArray;                    // first descriptor in array to update
            descriptorUpdateInfo.pImageInfo = &imageInfo; // descriptor refers to image data

            // Update descriptor.
            vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
        }

        return {};
    }

    std::optional<Error> GlslShaderTextureResourceBinding::onAfterAllPipelinesRefreshedResources() {
        std::scoped_lock guard(mtxPushConstantIndices.first, mtxUsedTexture.first);

        // Cast type.
        const auto pTextureResource = dynamic_cast<VulkanResource*>(mtxUsedTexture.second->getResource());
        if (pTextureResource == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource");
        }

        // Update push constant indices of all used pipelines.
        for (auto& [pPipeline, indices] : mtxPushConstantIndices.second) {
            // Convert pipeline.
            const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline);
            if (pVulkanPipeline == nullptr) [[unlikely]] {
                return Error("expected a Vulkan pipeline");
            }

            // Find a resource with our name in the descriptor set layout and update our index.
            auto pushConstantResult = pPipeline->getUintConstantOffset(getShaderResourceName());
            if (std::holds_alternative<Error>(pushConstantResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(pushConstantResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            indices.iPushConstantIndex = std::get<size_t>(pushConstantResult);

            // Bind image to descriptor.
            auto optionalError = bindTextureToShaderDescriptorArray(
                getShaderResourceName(),
                pVulkanPipeline,
                pTextureResource,
                indices.pShaderArrayIndex->getActualIndex());
            if (optionalError.has_value()) [[unlikely]] {
                auto error = std::move(optionalError.value());
                error.addCurrentLocationToErrorStack();
                return error;
            }
        }

        return {};
    }

    std::string GlslShaderTextureResourceBinding::getPathToTextureResource() {
        std::scoped_lock guard(mtxUsedTexture.first);

        return mtxUsedTexture.second->getPathToResourceRelativeRes();
    }

    std::optional<Error>
    GlslShaderTextureResourceBinding::useNewTexture(std::unique_ptr<TextureHandle> pTextureToUse) {
        std::scoped_lock guard(mtxPushConstantIndices.first, mtxUsedTexture.first);

        // Replace used texture.
        mtxUsedTexture.second = std::move(pTextureToUse);

        // Cast type.
        const auto pTextureResource = dynamic_cast<VulkanResource*>(mtxUsedTexture.second->getResource());
        if (pTextureResource == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource");
        }

        // Re-bind descriptors because they were re-created.
        for (const auto& [pVulkanPipeline, indices] : mtxPushConstantIndices.second) {
            auto optionalError = bindTextureToShaderDescriptorArray(
                getShaderResourceName(),
                pVulkanPipeline,
                pTextureResource,
                indices.pShaderArrayIndex->getActualIndex());
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }
        }

        return {};
    }

    std::optional<Error> GlslShaderTextureResourceBinding::changeUsedPipelines(
        const std::unordered_set<Pipeline*>& pipelinesToUse) {
        std::scoped_lock guard(mtxPushConstantIndices.first, mtxUsedTexture.first);

        // Make sure at least one pipeline is specified.
        if (pipelinesToUse.empty()) [[unlikely]] {
            return Error("expected at least one pipeline to be specified");
        }

        // Cast type.
        const auto pTextureResource = dynamic_cast<VulkanResource*>(mtxUsedTexture.second->getResource());
        if (pTextureResource == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource");
        }

        // Clear currently used pipelines.
        mtxPushConstantIndices.second.clear();

        for (const auto& pPipeline : pipelinesToUse) {
            // Convert pipeline.
            const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pPipeline);
            if (pVulkanPipeline == nullptr) [[unlikely]] {
                return Error("expected a Vulkan pipeline");
            }

            // Find a resource with our name in the descriptor set layout.
            auto pushConstantResult = pPipeline->getUintConstantOffset(getShaderResourceName());
            if (std::holds_alternative<Error>(pushConstantResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(pushConstantResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            const auto iPushConstantIndex = std::get<size_t>(pushConstantResult);

            // Get an index into the shader array.
            auto shaderArrayIndexResult =
                getTextureIndexInShaderArray(getShaderResourceName(), pVulkanPipeline);
            if (std::holds_alternative<Error>(shaderArrayIndexResult)) [[unlikely]] {
                auto error = std::get<Error>(std::move(shaderArrayIndexResult));
                error.addCurrentLocationToErrorStack();
                return error;
            }
            auto pShaderArrayIndex =
                std::get<std::unique_ptr<ShaderArrayIndex>>(std::move(shaderArrayIndexResult));

            // Bind image to descriptor.
            auto optionalError = bindTextureToShaderDescriptorArray(
                getShaderResourceName(),
                pVulkanPipeline,
                pTextureResource,
                pShaderArrayIndex->getActualIndex());
            if (optionalError.has_value()) [[unlikely]] {
                auto error = std::move(optionalError.value());
                error.addCurrentLocationToErrorStack();
                return error;
            }

            // Save a pair of "pipeline" - "index of push constant & array index".
            mtxPushConstantIndices.second[pVulkanPipeline] =
                PushConstantIndices(iPushConstantIndex, std::move(pShaderArrayIndex));
        }

        return {};
    }

    GlslShaderTextureResourceBinding::GlslShaderTextureResourceBinding(
        const std::string& sResourceName,
        std::unique_ptr<TextureHandle> pTextureToUse,
        std::unordered_map<VulkanPipeline*, PushConstantIndices> pushConstantIndices)
        : ShaderTextureResourceBinding(sResourceName) {
        // Save texture to use.
        mtxUsedTexture.second = std::move(pTextureToUse);

        // Save indices.
        mtxPushConstantIndices.second = std::move(pushConstantIndices);
    }
} // namespace ne
