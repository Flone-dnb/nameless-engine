#include "GlslComputeShaderInterface.h"

// Custom.
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "render/vulkan/resources/VulkanResource.h"
#include "render/vulkan/VulkanRenderer.h"

namespace ne {
    GlslComputeShaderInterface::GlslComputeShaderInterface(
        Renderer* pRenderer,
        const std::string& sComputeShaderName,
        bool bRunBeforeFrameRendering,
        ComputeExecutionGroup executionGroup)
        : ComputeShaderInterface(pRenderer, sComputeShaderName, bRunBeforeFrameRendering, executionGroup) {}

    std::optional<Error> GlslComputeShaderInterface::bindResource(
        GpuResource* pResource, const std::string& sShaderResourceName, ComputeResourceUsage usage) {
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

        // Make sure resource size information is available.
        if (pVulkanResource->getElementSizeInBytes() == 0 || pVulkanResource->getElementCount() == 0)
            [[unlikely]] {
            return Error("resource size information is not available");
        }

        // Get resource's internal VkBuffer to be used later.
        const auto pInternalVkBuffer = pVulkanResource->getInternalBufferResource();

        // Get renderer.
        const auto pVulkanRenderer = dynamic_cast<VulkanRenderer*>(getRenderer());
        if (pVulkanPipeline == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Get logical device.
        const auto pLogicalDevice = pVulkanRenderer->getLogicalDevice();
        if (pLogicalDevice == nullptr) [[unlikely]] {
            return Error("logical device is `nullptr`");
        }

        // Update one descriptor in set per frame resource.
        for (unsigned int i = 0; i < FrameResourcesManager::getFrameResourcesCount(); i++) {
            // Prepare info to bind storage buffer slot to descriptor.
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = pInternalVkBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = pVulkanResource->getElementSizeInBytes() * pVulkanResource->getElementCount();

            // Bind reserved space to descriptor.
            VkWriteDescriptorSet descriptorUpdateInfo{};
            descriptorUpdateInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorUpdateInfo.dstSet =
                pMtxPipelineInternalResources->second.vDescriptorSets[i]; // descriptor set to update
            descriptorUpdateInfo.dstBinding = iBindingIndex;              // descriptor binding index
            descriptorUpdateInfo.dstArrayElement = 0; // first descriptor in array to update
            descriptorUpdateInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            descriptorUpdateInfo.descriptorCount = 1;       // how much descriptors in array to update
            descriptorUpdateInfo.pBufferInfo = &bufferInfo; // descriptor refers to buffer data

            // Update descriptor.
            vkUpdateDescriptorSets(pLogicalDevice, 1, &descriptorUpdateInfo, 0, nullptr);
        }

        return {};
    }

}
