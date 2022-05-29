﻿#pragma once

// STL.
#include <unordered_map>

// Custom.
#include "shaders/ShaderDescription.h"

namespace ne {
    class ShaderManager;

    /**
     * The only class that is allowed to store shaders (not counting ShaderManager).
     *
     * Other classes should inherit from this class in order to work with shaders.
     *
     * Allows storing one shader per shader type.
     */
    class ShaderUser {
    public:
        ShaderUser() = delete;
        ShaderUser(const ShaderUser&) = delete;
        ShaderUser& operator=(const ShaderUser&) = delete;

    protected:
        /**
         * Constructor.
         *
         * @param pShaderManager Shader manager to use.
         */
        ShaderUser(ShaderManager* pShaderManager);
        virtual ~ShaderUser();

        /**
         * Adds a shader to be stored.
         *
         * @warning If a shader of this type was already added it will be replaced with the new one.
         *
         * @param sShaderName Name of the compiled shader (see ShaderManager::compileShaders).
         *
         * @return 'false' if shader was added successfully, 'true' if it was not found in ShaderManager.
         */
        bool addShader(const std::string& sShaderName);

        /**
         * Returns previously added shader (@ref addShader) for the specified type.
         *
         * @warning Returned shader will not be destroyed until this ShaderUser object is not destroyed.
         * Once this ShaderUser object is destroyed there is no guarantee that the shader will be valid.
         *
         * @param shaderType Type of the shader to query.
         *
         * @return empty if a shader of this type was not added before, valid pointer otherwise.
         */
        std::optional<std::weak_ptr<IShader>> getShader(ShaderType shaderType) const;

    private:
        /** Stored shaders. */
        std::unordered_map<ShaderType, std::shared_ptr<IShader>> shaders;

        /** Shader manager to work with shaders. */
        ShaderManager* pShaderManager;
    };
} // namespace ne
