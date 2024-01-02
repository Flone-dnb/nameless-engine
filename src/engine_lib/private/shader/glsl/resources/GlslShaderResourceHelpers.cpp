#include "GlslShaderResourceHelpers.h"

// Custom.
#include "render/vulkan/pipeline/VulkanPipeline.h"

namespace ne {

    std::variant<size_t, Error> GlslShaderResourceHelpers::getPushConstantIndex(
        VulkanPipeline* pVulkanPipeline, const std::string& sShaderResourceName) {
        // Get pipeline's push constants and internal resources.
        const auto pMtxPipelineResources = pVulkanPipeline->getInternalResources();
        const auto pMtxPushConstants = pVulkanPipeline->getShaderConstants();

        // Lock both.
        std::scoped_lock guard(pMtxPipelineResources->first, pMtxPushConstants->first);

        // Find a shader resource binding using the specified name.
        const auto bindingIt = pMtxPipelineResources->second.resourceBindings.find(sShaderResourceName);
        if (bindingIt == pMtxPipelineResources->second.resourceBindings.end()) [[unlikely]] {
            return Error(std::format(
                "unable to find a shader resource by the specified name \"{}\", make sure the resource name "
                "is correct and that this resource is actually being used inside of your shader (otherwise "
                "the shader resource might be optimized out and the engine will not be able to see it)",
                sShaderResourceName));
        }

        // Make sure push constants are used.
        if (!pMtxPushConstants->second.has_value()) [[unlikely]] {
            return Error("expected push constants to be used");
        }

        auto& uintConstantsOffsets = pMtxPushConstants->second->uintConstantsOffsets;

        // Make sure that name of this shader resources exists as a field in push constants (in GLSL).
        const auto fieldNameIt = uintConstantsOffsets.find(sShaderResourceName);
        if (fieldNameIt == uintConstantsOffsets.end()) [[unlikely]] {
            return Error(std::format(
                "expected to find a shader resource \"{}\" to be referenced in push constant (in GLSL)",
                sShaderResourceName));
        }

        return fieldNameIt->second;
    }

}
