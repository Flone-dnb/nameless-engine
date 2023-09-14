#include "HlslShaderResourceHelpers.h"

// Custom.
#include "render/directx/pipeline/DirectXPso.h"

namespace ne {

    std::variant<unsigned int, Error> HlslShaderResourceHelpers::getRootParameterIndexFromPipeline(
        Pipeline* pPipeline, const std::string& sShaderResourceName) {
        // Make sure PSO has correct type.
        const auto pDirectXPso = dynamic_cast<DirectXPso*>(pPipeline);
        if (pDirectXPso == nullptr) [[unlikely]] {
            return Error("expected DirectX PSO");
        }

        // Get PSO internal resources.
        auto pMtxInternalPsoResources = pDirectXPso->getInternalResources();
        std::scoped_lock psoGuard(pMtxInternalPsoResources->first);

        // Find this resource by name.
        auto it = pMtxInternalPsoResources->second.rootParameterIndices.find(sShaderResourceName);
        if (it == pMtxInternalPsoResources->second.rootParameterIndices.end()) [[unlikely]] {
            return Error(std::format(
                "unable to find a shader resource by the specified name \"{}\", make sure the resource name "
                "is correct and that this resource is actually being used inside of your shader (otherwise "
                "the shader resource might be optimized out and the engine will not be able to see it)",
                sShaderResourceName));
        }

        return it->second;
    }

}
