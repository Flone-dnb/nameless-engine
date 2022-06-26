#include "ShaderUser.h"

// STL.
#include <ranges>

// Custom.
#include "shaders/ShaderManager.h"

namespace ne {
    ShaderUser::ShaderUser(ShaderManager* pShaderManager) { this->pShaderManager = pShaderManager; }

    bool ShaderUser::addShader(const std::string& sShaderName) {
        const auto optional = pShaderManager->getShader(sShaderName);
        if (!optional.has_value()) {
            return true;
        }

        auto pShader = optional.value();
        const auto shaderType = pShader->getShaderType();

        const auto it = shaders.find(shaderType);
        if (it != shaders.end()) {
            // Already have a shader of this type. Replace old one.
            const auto sOldShaderName = it->second->getShaderName();

            shaders[shaderType] = std::move(pShader);

            releaseShader(sOldShaderName);
        } else {
            shaders[shaderType] = std::move(pShader);
        }

        return false;
    }

    std::optional<IShaderPack*> ShaderUser::getShader(ShaderType shaderType) const {
        const auto it = shaders.find(shaderType);
        if (it == shaders.end()) {
            return {};
        }

        return it->second.get();
    }

    void ShaderUser::releaseShader(const std::string& sShaderName) const {
        pShaderManager->releaseShaderBytecodeIfNotUsed(sShaderName);
        pShaderManager->removeShaderIfMarkedToBeRemoved(sShaderName);
    }

    ShaderUser::~ShaderUser() {
        std::vector<std::string> vShaderNamesToRemove;

        for (const auto& shader : shaders | std::views::values) {
            vShaderNamesToRemove.push_back(shader->getShaderName());
        }

        // Clear shaders so that we don't hold any references now
        // and ShaderManager can release those shaders as no one is referencing them.
        shaders.clear();

        for (const auto& sShaderName : vShaderNamesToRemove) {
            releaseShader(sShaderName);
        }
    }
} // namespace ne
