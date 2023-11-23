#include "HlslComputeShaderInterface.h"

// Custom.
#include "render/Renderer.h"
#include "render/directx/resources/DirectXResourceManager.h"

namespace ne {
    HlslComputeShaderInterface::HlslComputeShaderInterface(
        Renderer* pRenderer,
        const std::string& sComputeShaderName,
        ComputeExecutionStage executionStage,
        ComputeExecutionGroup executionGroup)
        : ComputeShaderInterface(pRenderer, sComputeShaderName, executionStage, executionGroup) {
        // Get resource manager.
        const auto pResourceManager = dynamic_cast<DirectXResourceManager*>(pRenderer->getResourceManager());
        if (pResourceManager == nullptr) [[unlikely]] {
            Error error("expected a DirectX resource manager to be valid");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
        }

        // Save heaps.
        pCbvSrvUavHeap = pResourceManager->getCbvSrvUavHeap();

        // Save SRV descriptor size.
        iCbvSrvUavDescriptorSize = pCbvSrvUavHeap->getDescriptorSize();
    }

    std::optional<Error> HlslComputeShaderInterface::bindResource(
        GpuResource* pResource,
        const std::string& sShaderResourceName,
        ComputeResourceUsage usage,
        bool bUpdateOnlyCurrentFrameResourceDescriptors) {
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
        case (ComputeResourceUsage::CONSTANT_BUFFER): {
            // Bind CBV.
            auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::CBV);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }

            // Add to CBV array.
            cbvResources[iRootParameterIndex] = pDirectXResource;
            break;
        }
        case (ComputeResourceUsage::READ_ONLY_TEXTURE): {
            // Bind SRV.
            auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::SRV);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }

            // Get descriptor.
            const auto pDescriptor = pDirectXResource->getDescriptor(DirectXDescriptorType::SRV);
            if (pDescriptor == nullptr) [[unlikely]] {
                return Error("expected descriptor to be valid");
            }

            // Add to SRV table array.
            tableResources[iRootParameterIndex] = pDescriptor;
            break;
        }
        case (ComputeResourceUsage::READ_WRITE_TEXTURE): {
            // Bind UAV.
            auto optionalError = pDirectXResource->bindDescriptor(DirectXDescriptorType::UAV);
            if (optionalError.has_value()) [[unlikely]] {
                optionalError->addCurrentLocationToErrorStack();
                return optionalError;
            }

            // Get descriptor.
            const auto pDescriptor = pDirectXResource->getDescriptor(DirectXDescriptorType::UAV);
            if (pDescriptor == nullptr) [[unlikely]] {
                return Error("expected descriptor to be valid");
            }

            // Add to table array.
            tableResources[iRootParameterIndex] = pDescriptor;
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
