#include "DirectXPso.h"

// Custom.
#include "render/directx/DirectXRenderer.h"
#include "io/Logger.h"

namespace ne {
    DirectXPso::DirectXPso(DirectXRenderer* pRenderer) : ShaderUser(pRenderer->getShaderManager()) {
        this->pRenderer = pRenderer;
    }

    bool DirectXPso::assignShader(const std::string& sShaderName) { return addShader(sShaderName); }
} // namespace ne
