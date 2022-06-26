#include "IShaderPack.h"

namespace ne {
    IShaderPack::IShaderPack(const std::string& sShaderName) { this->sShaderName = sShaderName; }

    std::string IShaderPack::getShaderName() const { return sShaderName; }
} // namespace ne
