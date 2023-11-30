#include "GlslComputeShaderInterface.h"

// Custom.
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "render/vulkan/resources/VulkanResource.h"
#include "render/vulkan/VulkanRenderer.h"

namespace ne {
    GlslComputeShaderInterface::GlslComputeShaderInterface(
        Renderer* pRenderer,
        const std::string& sComputeShaderName,
        ComputeExecutionStage executionStage,
        ComputeExecutionGroup executionGroup)
        : ComputeShaderInterface(pRenderer, sComputeShaderName, executionStage, executionGroup) {}

    std::optional<Error> GlslComputeShaderInterface::bindResource(
        GpuResource* pResource,
        const std::string& sShaderResourceName,
        ComputeResourceUsage usage,
        bool bUpdateOnlyCurrentFrameResourceDescriptors) {
        // Get pipeline.
        const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(getPipeline());
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan pipeline");
        }

        // Get pipeline's internal resources.
        const auto pMtxPipelineInternalResources = pVulkanPipeline->getInternalResources();
        std::scoped_lock pipelineResourcesGuard(pMtxPipelineInternalResources->first);

        // Find binding index for the specified shader resource.
        auto bindingIndeIt = pMtxPipelineInternalResources->second.resourceBindings.find(sShaderResourceName);
        if (bindingIndeIt == pMtxPipelineInternalResources->second.resourceBindings.end()) [[unlikely]] {
            return Error(std::format(
                "unable to find a shader resource with the name \"{}\" in the pipeline \"{}\", make sure "
                "this resource is actually being used in your shader and is not optimized out by the "
                "compiler",
                sShaderResourceName,
                pVulkanPipeline->getPipelineIdentifier()));
        }
        const auto iBindingIndex = bindingIndeIt->second;

        // Convert resource.
        const auto pVulkanResource = dynamic_cast<VulkanResource*>(pResource);
        if (pVulkanResource == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource");
        }

        // Prepare image layout for image descriptors.
        auto imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Prepare descriptor type.
        bool bIsBufferDescriptor = true;
        VkDescriptorType descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        switch (usage) {
        case (ComputeResourceUsage::CONSTANT_BUFFER): {
            descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            break;
        }
        case (ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER): {
            descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        }
        case (ComputeResourceUsage::READ_WRITE_ARRAY_BUFFER): {
            descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            break;
        }
        case (ComputeResourceUsage::READ_ONLY_TEXTURE): {
            descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bIsBufferDescriptor = false;
            break;
        }
        case (ComputeResourceUsage::READ_WRITE_TEXTURE): {
            descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            bIsBufferDescriptor = false;
            break;
        }
        default: {
            return Error("unhandled case");
            break;
        }
        }

        if (bIsBufferDescriptor) {
            // Make sure resource size information is available.
            if (pVulkanResource->getElementSizeInBytes() == 0 || pVulkanResource->getElementCount() == 0)
                [[unlikely]] {
                return Error("resource size information is not available");
            }

            // Make sure buffer is valid.
            if (pVulkanResource->getInternalBufferResource() == nullptr) [[unlikely]] {
                return Error("expected resource's buffer to be valid");
            }
        } else {
            // Make sure image view is valid.
            if (pVulkanResource->getInternalImageView() == nullptr) [[unlikely]] {
                return Error("expected resource's image view to be valid");
            }
        }

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Get compute sampler (make sure it's valid).
        const auto pComputeSampler = pVulkanRenderer->getComputeTextureSampler();
        if (pComputeSampler == nullptr) [[unlikely]] {
            return Error("expected compute sampler to be valid");
        }

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("logical device is `nullptr`");
        }

        // Prepare indices of frame resources to update.
        std::vector<unsigned int> vFrameResourcesToUpdate;
        vFrameResourcesToUpdate.reserve(FrameResourcesManager::getFrameResourcesCount());

        // Get current frame resource.
        const auto pMtxCurrentFrameResource =
            pVulkanRenderer->getFrameResourcesManager()->getCurrentFrameResource();
        std::scoped_lock frameResourceGuard(pMtxCurrentFrameResource->first);

        // Prepare frame resources to update.
        if (bUpdateOnlyCurrentFrameResourceDescriptors) {
            vFrameResourcesToUpdate.push_back(
                static_cast<unsigned int>(pMtxCurrentFrameResource->second.iCurrentFrameResourceIndex));
        } else {
            for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
                vFrameResourcesToUpdate.push_back(i);
            }
        }

        // Update descriptors.
        for (auto& iFrameResourceIndex : vFrameResourcesToUpdate) {
            VkDescriptorBufferInfo bufferInfo{};
            VkDescriptorImageInfo imageInfo{};

            if (bIsBufferDescriptor) {
                // Prepare info to bind storage buffer slot to descriptor.
                bufferInfo.buffer = pVulkanResource->getInternalBufferResource();
                bufferInfo.offset = 0;
                bufferInfo.range =
                    pVulkanResource->getElementSizeInBytes() * pVulkanResource->getElementCount();
            } else {
                // Prepare info to bind an image view to descriptor.
                imageInfo.imageLayout = imageLayout;
                if (pVulkanResource->getInternalImageViewDepthAspect() != nullptr) {
                    // Don't use a view that references both depth and stencil aspects as this is an error.
                    imageInfo.imageView = pVulkanResource->getInternalImageViewDepthAspect();
                } else {
                    imageInfo.imageView = pVulkanResource->getInternalImageView();
                }
                imageInfo.sampler = pComputeSampler;
            }

            // Bind reserved space to descriptor.
            VkWriteDescriptorSet descriptorUpdateInfo{};
            descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorUpdateInfo.dstSet =
                pMtxPipelineInternalResources->second
                    .vDescriptorSets[iFrameResourceIndex];   // descriptor set to update
            descriptorUpdateInfo.dstBinding = iBindingIndex; // descriptor binding index
            descriptorUpdateInfo.dstArrayElement = 0;        // first descriptor in array to update
            descriptorUpdateInfo.descriptorType = descriptorType;
            descriptorUpdateInfo.descriptorCount = 1; // how much descriptors in array to update

            if (bIsBufferDescriptor) {
                descriptorUpdateInfo.pBufferInfo = &bufferInfo; // descriptor refers to buffer data
            } else {
                descriptorUpdateInfo.pImageInfo = &imageInfo; // descriptor refers to image data
            }

            // Update descriptor.
            vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
        }

        return {};
    }

}
