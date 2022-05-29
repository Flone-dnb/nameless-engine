#pragma once

// Custom.
#include "shaders/hlsl/HlslShader.h"
#include "shaders/ShaderUser.h"

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXRenderer;

    /** Our DirectX pipeline state object (PSO) wrapper. */
    class Pso : public ShaderUser {
    public:
        /**
         * Constructor.
         *
         * @param pRenderer Parent renderer that owns this PSO.
         */
        Pso(DirectXRenderer* pRenderer);
        Pso() = delete;
        Pso(const Pso&) = delete;
        Pso& operator=(const Pso&) = delete;

        virtual ~Pso() override {}

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
