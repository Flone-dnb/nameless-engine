#include "ShaderUser.h"

// STL.
#include <ranges>

// Custom.
#include "shaders/ShaderManager.h"
#include "shaders/IShader.h"

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

            pShaderManager->releaseShaderBytecodeIfNotUsed(sOldShaderName);
            pShaderManager->removeShaderIfMarkedToBeRemoved(sOldShaderName);
        } else {
            shaders[shaderType] = std::move(pShader);
        }

        return false;
    }

    std::optional<std::weak_ptr<IShader>> ShaderUser::getShader(ShaderType shaderType) const {
        const auto it = shaders.find(shaderType);
        if (it == shaders.end()) {
            return {};
        }

        return it->second;
    }

    ShaderUser::~ShaderUser() {
        std::vector<std::string> vShaderNamesToRemove;

        for (const auto& shader : shaders | std::views::values) {
            vShaderNamesToRemove.push_back(shader->getShaderName());
        }

        shaders.clear();

        for (const auto& sShaderName : vShaderNamesToRemove) {
            pShaderManager->releaseShaderBytecodeIfNotUsed(sShaderName);
            pShaderManager->removeShaderIfMarkedToBeRemoved(sShaderName);
        }
    }
} // namespace ne
