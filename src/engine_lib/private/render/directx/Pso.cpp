#include "Pso.h"

// STL.
#include <ranges>

// Custom.
#include "render/directx/DirectXRenderer.h"
#include "shaders/ShaderManager.h"
#include "io/Logger.h"

namespace ne {
    Pso::Pso(DirectXRenderer* pRenderer) { this->pRenderer = pRenderer; }

    Pso::~Pso() {
        // Check if we need to release shaders.
        for (const auto& shader : shaders | std::views::values) {
            if (shader.use_count() == 2) {
                // There are only 2 pointers to this shader: here and in ShaderManager.
                shader->releaseFromMemory();
            }
        }
    }

    bool Pso::assignShader(const std::string& sShaderName) {
        const auto optional = pRenderer->getShaderManager()->getShader(sShaderName);
        if (!optional.has_value()) {
            return true;
        }

        auto pShader = std::dynamic_pointer_cast<HlslShader>(optional.value());

        shaders[pShader->getShaderType()] = std::move(pShader);

        return false;
    }
} // namespace ne
