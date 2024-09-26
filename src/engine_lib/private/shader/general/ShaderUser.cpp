#include "ShaderUser.h"

// Custom.
#include "shader/ShaderManager.h"

namespace ne {
    ShaderUser::ShaderUser(ShaderManager* pShaderManager) { this->pShaderManager = pShaderManager; }

    bool ShaderUser::addShader(const std::string& sShaderName) {
        std::scoped_lock guard(mtxAssignedShaders.first);

        // See if we already assigned the requested shader.
        for (const auto& [shaderType, pShader] : mtxAssignedShaders.second) {
            if (sShaderName == pShader->getShaderName()) {
                return false; // do nothing
            }
        }

        // Get the requested shader.
        auto pShader = pShaderManager->findShader(sShaderName);
        if (pShader == nullptr) {
            // Not found.
            return true;
        }
        const auto shaderType = pShader->getShaderType();

        // See if we already assigned a shader of this type.
        const auto it = mtxAssignedShaders.second.find(shaderType);
        if (it != mtxAssignedShaders.second.end()) {
            // Already have a shader of this type. Replace old one.
            const auto sOldShaderName = it->second->getShaderName();

            mtxAssignedShaders.second[shaderType] = std::move(pShader);

            releaseShader(sOldShaderName);
        } else {
            mtxAssignedShaders.second[shaderType] = std::move(pShader);
        }

        return false;
    }

    ShaderPack* ShaderUser::findShader(ShaderType shaderType) {
        std::scoped_lock guard(mtxAssignedShaders.first);

        const auto it = mtxAssignedShaders.second.find(shaderType);
        if (it == mtxAssignedShaders.second.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    void ShaderUser::releaseShader(const std::string& sShaderName) const {
        pShaderManager->releaseShaderBytecodeIfNotUsed(sShaderName);
        pShaderManager->removeShaderIfMarkedToBeRemoved(sShaderName);
    }

    ShaderUser::~ShaderUser() {
        std::scoped_lock guard(mtxAssignedShaders.first);

        std::vector<std::string> vShaderNamesToRemove;

        for (const auto& [key, shader] : mtxAssignedShaders.second) {
            vShaderNamesToRemove.push_back(shader->getShaderName()); // NOLINT
        }

        // Clear shaders so that we don't hold any references now
        // and ShaderManager can release those shaders as no one is referencing them.
        mtxAssignedShaders.second.clear();

        for (const auto& sShaderName : vShaderNamesToRemove) {
            releaseShader(sShaderName);
        }
    }
} // namespace ne
