#pragma once

// Custom.
#include "shaders/hlsl/HlslShader.h"
#include "shaders/ShaderUser.h"

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXRenderer;

    /** Our DirectX pipeline state object (PSO) wrapper. */
    class DirectXPso : public ShaderUser {
    public:
        /**
         * Constructor.
         *
         * @param pRenderer Parent renderer that owns this PSO.
         */
        DirectXPso(DirectXRenderer* pRenderer);
        DirectXPso() = delete;
        DirectXPso(const DirectXPso&) = delete;
        DirectXPso& operator=(const DirectXPso&) = delete;

        virtual ~DirectXPso() override {}

        /**
         * Assigns a shader to PSO.
         *
         * @warning If a shader of this type was already added it will be replaced with the new one.
         *
         * @param sShaderName Name of the compiled shader (see ShaderManager::compileShaders).
         *
         * @return 'false' if shader was assigned successfully, 'true' if it was not found in ShaderManager.
         */
        bool assignShader(const std::string& sShaderName);

    private:
        /** Do not delete. Parent renderer that uses this PSO. */
        DirectXRenderer* pRenderer;

        // TODO: add actual PSO here and provide a getter.
    };
} // namespace ne
