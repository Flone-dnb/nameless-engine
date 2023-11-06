#include "HlslComputeShaderInterface.h"

// Custom.
#include "render/Renderer.h"

namespace ne {
    HlslComputeShaderInterface::HlslComputeShaderInterface(
        Renderer* pRenderer,
        const std::string& sComputeShaderName,
        bool bRunBeforeFrameRendering,
        ComputeExecutionGroup executionGroup)
        : ComputeShaderInterface(pRenderer, sComputeShaderName, bRunBeforeFrameRendering, executionGroup) {}

    std::optional<Error> HlslComputeShaderInterface::bindResource(
        GpuResource* pResource, const std::string& sShaderResourceName, ComputeResourceUsage usage) {
        // Convert resource.
        const auto pDirectXResource = dynamic_cast<DirectXResource*>(pResource);
        if (pDirectXResource == nullptr) [[unlikely]] {
            return Error("expected a DirectX resource");
        }

        // Get pipeline.
        const auto pPso = dynamic_cast<DirectXPso*>(getPipeline());
        if (pPso == nullptr) [[unlikely]] {
            return Error("expected a DirectX PSO");
        }

        // Find the specified resource name in the root signature.
        UINT iRootParameterIndex = 0;
        {
            const auto pMtxPsoResources = pPso->getInternalResources();
            std::scoped_lock guard(pMtxPsoResources->first);

            // Find root parameter for this shader resource.
            auto rootParameterIt = pMtxPsoResources->second.rootParameterIndices.find(sShaderResourceName);
            if (rootParameterIt == pMtxPsoResources->second.rootParameterIndices.end()) [[unlikely]] {
                return Error(std::format(
                    "unable to find a shader resource with the name \"{}\" in the pipeline \"{}\", make sure "
                    "this resource is actually being used in your shader and is not optimized out by the "
                    "compiler",
                    sShaderResourceName,
                    pPso->getPipelineIdentifier()));
            }

            // Save found index.
            iRootParameterIndex = rootParameterIt->second;
        }

        // Bind descriptor according to the resource usage.
        switch (usage) {
        case (ComputeResourceUsage::READ_ONLY_ARRAY_BUFFER): {
            // Bind SRV.
            auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::SRV);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }

            // Add to SRV array.
            srvResources[iRootParameterIndex] = pDirectXResource;
            break;
        }
        case (ComputeResourceUsage::READ_WRITE_ARRAY_BUFFER): {
            // Bind UAV.
            auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::UAV);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }

            // Add to UAV array.
            uavResources[iRootParameterIndex] = pDirectXResource;
            break;
        }
        default: {
            return Error("unhandled case");
            break;
        }
        }

        return {};
    }

}
