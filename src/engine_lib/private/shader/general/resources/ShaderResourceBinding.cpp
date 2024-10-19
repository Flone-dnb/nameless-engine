#include "ShaderResourceBinding.h"

// Custom.
#include "render/general/pipeline/Pipeline.h"

namespace ne {

    ShaderResourceBindingBase::ShaderResourceBindingBase(const std::string& sShaderResourceName)
        : sShaderResourceName(sShaderResourceName) {}

    std::string ShaderResourceBindingBase::getShaderResourceName() const { return sShaderResourceName; }

    ShaderTextureResourceBinding::ShaderTextureResourceBinding(const std::string& sShaderResourceName)
        : ShaderResourceBindingBase(sShaderResourceName) {}

} // namespace ne
