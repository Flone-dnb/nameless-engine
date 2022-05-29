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
            // Already have a shader of this type.
            const auto sOldShaderName = it->second->getShaderName();
            bool bCheckToRemoveOldShader = false;

            if (pShaderManager->getShaderUseCount(sOldShaderName) == 2) {
                // There are only 2 pointers to this shader: here and in ShaderManager.
                it->second->releaseBytecodeFromMemory();
                bCheckToRemoveOldShader = true;
            }

            shaders[shaderType] = std::move(pShader);

            if (bCheckToRemoveOldShader) {
                pShaderManager->removeShaderIfMarkedToBeRemoved(sOldShaderName);
            }
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

        // Check if we need to release shaders.
        for (const auto& shader : shaders | std::views::values) {
            if (pShaderManager->getShaderUseCount(shader->getShaderName()) == 2) {
                // There are only 2 pointers to this shader: here and in ShaderManager.
                shader->releaseBytecodeFromMemory();
                vShaderNamesToRemove.push_back(shader->getShaderName());
            }
        }

        shaders.clear();

        for (const auto& sShaderName : vShaderNamesToRemove) {
            // Now it's only 1 pointer to shader (left in ShaderManager).
            pShaderManager->removeShaderIfMarkedToBeRemoved(sShaderName);
        }
    }
} // namespace ne
