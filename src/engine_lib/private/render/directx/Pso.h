#pragma once

// STL.
#include <unordered_map>
#include <memory>

// Custom.
#include "shaders/hlsl/HlslShader.h"

namespace ne {
    using namespace Microsoft::WRL;

    class DirectXRenderer;

    /** Our DirectX pipeline state object (PSO) wrapper. */
    class Pso {
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

        ~Pso();

        /**
         * Assigns a shader to PSO.
         *
         * @param sShaderName Name of the compiled shader (see ShaderManager::compileShaders).
         *
         * @return 'false' if shader was assigned successfully, 'true' if it was not found in ShaderManager.
         */
        bool assignShader(const std::string& sShaderName);

    private:
        /** Shaders used in this PSO. */
        std::unordered_map<ShaderType, std::shared_ptr<HlslShader>> shaders;

        /** Do not delete. Parent renderer that uses this PSO. */
        DirectXRenderer* pRenderer;

        // TODO: add actual PSO here and provide a getter.
    };
} // namespace ne
