#include "ShaderResource.h"

// Custom.
#include "render/general/pipeline/Pipeline.h"

namespace ne {

    ShaderResourceBase::ShaderResourceBase(const std::string& sResourceName) {
        this->sResourceName = sResourceName;
    }

    std::string ShaderResourceBase::getResourceName() const { return sResourceName; }

    ShaderTextureResource::ShaderTextureResource(const std::string& sResourceName)
        : ShaderResourceBase(sResourceName) {}

} // namespace ne
