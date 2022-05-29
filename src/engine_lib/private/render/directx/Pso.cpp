#include "Pso.h"

// Custom.
#include "render/directx/DirectXRenderer.h"
#include "io/Logger.h"

namespace ne {
    Pso::Pso(DirectXRenderer* pRenderer) : ShaderUser(pRenderer->getShaderManager()) {
        this->pRenderer = pRenderer;
    }

    bool Pso::assignShader(const std::string& sShaderName) { return addShader(sShaderName); }
} // namespace ne
