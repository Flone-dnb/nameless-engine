#include "GlslGlobalShaderResourceBinding.h"

// Custom.
#include "render/general/resource/GpuResourceManager.h"
#include "render/general/pipeline/PipelineManager.h"
#include "render/vulkan/VulkanRenderer.h"
#include "render/vulkan/pipeline/VulkanPipeline.h"
#include "render/vulkan/resource/VulkanResource.h"
#include "misc/Profiler.hpp"

namespace ne {

    GlslGlobalShaderResourceBinding::GlslGlobalShaderResourceBinding(
        GlobalShaderResourceBindingManager* pManager,
        const std::string& sShaderResourceName,
        const std::array<GpuResource*, FrameResourceManager::getFrameResourceCount()>& vResourcesToBind)
        : GlobalShaderResourceBinding(pManager, sShaderResourceName, vResourcesToBind) {}

    GlslGlobalShaderResourceBinding::~GlslGlobalShaderResourceBinding() { unregisterBinding(); }

    std::optional<Error> GlslGlobalShaderResourceBinding::bindToPipelines(Pipeline* pSpecificPipeline) {
        PROFILE_FUNC;

        const auto vResourcesToBind = getBindedResources();

        // Get renderer.
        const auto pVulkanRenderer =
            dynamic_cast<VulkanRenderer*>(vResourcesToBind[0]->getResourceManager()->getRenderer());
        if (pVulkanRenderer == nullptr) [[unlikely]] {
            return Error("expected a Vulkan renderer");
        }

        // Get pipeline manager.
        const auto pPipelineManager = pVulkanRenderer->getPipelineManager();
        if (pPipelineManager == nullptr) [[unlikely]] {
            return Error("pipeline manager is `nullptr`");
        }

        // Cast type.
        const auto pVulkanResource = dynamic_cast<VulkanResource*>(vResourcesToBind[0]);
        if (pVulkanResource == nullptr) [[unlikely]] {
            return Error("expected a Vulkan resource");
        }

        if (pVulkanResource->getInternalImage() == nullptr) {
            // Buffer resource.
            const VkDescriptorType descriptorType = pVulkanResource->isStorageResource()
                                                        ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                                                        : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

            if (pSpecificPipeline == nullptr) {
                // Bind to all pipelines.
                auto optionalError = pPipelineManager->bindBuffersToAllVulkanPipelinesIfUsed(
                    vResourcesToBind, getShaderResourceName(), descriptorType);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = std::move(optionalError.value());
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            } else {
                // Bind to specific pipeline.
                const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pSpecificPipeline);
                if (pVulkanPipeline == nullptr) [[unlikely]] {
                    return Error("expected a Vulkan pipeline");
                }

                auto optionalError = pVulkanPipeline->bindBuffersIfUsed(
                    vResourcesToBind, getShaderResourceName(), descriptorType);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = std::move(optionalError.value());
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            }
        } else {
            // Since it's an image make sure all pointers in the array point to the same resource.
            const auto pImageResource = vResourcesToBind[0];
            for (const auto& pResource : vResourcesToBind) {
                if (pImageResource != pResource) [[unlikely]] {
                    return Error(std::format(
                        "expected the global image shader resource \"{}\" (binding to shader resource "
                        "\"{}\") to be the same for all frames in-flight",
                        pImageResource->getResourceName(),
                        getShaderResourceName()));
                }
            }

            // Image resource.
            const VkDescriptorType descriptorType = pVulkanResource->isStorageResource()
                                                        ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                                        : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            const VkImageLayout layout = pVulkanResource->isStorageResource()
                                             ? VK_IMAGE_LAYOUT_GENERAL
                                             : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            // Get render settings.
            const auto mtxRenderSettings = pPipelineManager->getRenderer()->getRenderSettings();
            std::scoped_lock guard(*mtxRenderSettings.first);

            // Get current texture filtering.
            const auto textureFilteringQuality = mtxRenderSettings.second->getTextureFilteringQuality();

            // Get texture sampler.
            const auto pTextureSampler = pVulkanRenderer->getTextureSampler(textureFilteringQuality);
            if (pTextureSampler == nullptr) [[unlikely]] {
                return Error("texture sampler is `nullptr`");
            }

            if (pSpecificPipeline == nullptr) {
                // Bind to all pipelines.
                auto optionalError = pPipelineManager->bindImageToAllVulkanPipelinesIfUsed(
                    pImageResource, getShaderResourceName(), descriptorType, layout, pTextureSampler);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = std::move(optionalError.value());
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            } else {
                // Bind to specific pipeline.
                const auto pVulkanPipeline = dynamic_cast<VulkanPipeline*>(pSpecificPipeline);
                if (pVulkanPipeline == nullptr) [[unlikely]] {
                    return Error("expected a Vulkan pipeline");
                }

                auto optionalError = pVulkanPipeline->bindImageIfUsed(
                    pImageResource, getShaderResourceName(), descriptorType, layout, pTextureSampler);
                if (optionalError.has_value()) [[unlikely]] {
                    auto error = std::move(optionalError.value());
                    error.addCurrentLocationToErrorStack();
                    return error;
                }
            }
        }

        return {};
    }

}
