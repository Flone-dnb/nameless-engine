#pragma once

// STL.
#include <mutex>

// Custom.
#include "shaders/IShader.h"
#include "render/IRenderer.h"

// External.
#include "DirectXShaderCompiler/inc/dxcapi.h"

// OS.
#include <wrl.h>

namespace ne {
    using namespace Microsoft::WRL;

    /**
     * Represents compiled HLSL shader.
     */
    class HlslShader : public IShader {
    public:
        /**
         * Constructor.
         *
         * @param pathToCompiledShader Path to compiled shader blob on disk.
         * @param sShaderName          Unique name of this shader.
         * @param shaderType           Type of this shader.
         */
        HlslShader(
            std::filesystem::path pathToCompiledShader,
            const std::string& sShaderName,
            ShaderType shaderType);

        HlslShader() = delete;
        HlslShader(const HlslShader&) = delete;
        HlslShader& operator=(const HlslShader&) = delete;

        virtual ~HlslShader() override {}

        /**
         * Compiles a shader.
         *
         * @param pRenderer         DirectX renderer.
         * @param shaderDescription Description that describes the shader and how the shader should be
         * compiled.
         *
         * @return Returns one of the three values:
         * @arg compiled shader
         * @arg string containing shader compilation error/warning
         * @arg internal error
         */
        static std::variant<
            std::shared_ptr<IShader> /** Compiled shader. */,
            std::string /** Compilation error. */,
            Error /** Internal error. */>
        compileShader(IRenderer* pRenderer, const ShaderDescription& shaderDescription);

        /**
         * Loads compiled bytecode from disk and stores it in memory.
         * Subsequent calls to this function will just copy pointer
         * to bytecode from memory (no disk loading will happen).
         *
         * @return Compiled shader blob.
         */
        std::variant<ComPtr<IDxcBlob>, Error> getCompiledBlob();

        /**
         * Releases underlying shader bytecode from memory (this object will not be deleted)
         * if the shader bytecode was loaded into memory.
         * Next time this shader will be needed it will be loaded from disk.
         *
         * @return 'false' if was released from memory, 'true' if not loaded into memory.
         */
        virtual bool releaseBytecodeFromMemoryIfLoaded() override;

    private:
        friend class ShaderManager;

        /**
         * Mutex for read/write operations on compiled blob.
         * Compiled shader bytecode (may be empty if not stored in memory right now).
         */
        std::pair<std::mutex, ComPtr<IDxcBlob>> mtxCompiledBlob;

        /** Shader file encoding. */
        static inline UINT iShaderFileCodepage = DXC_CP_ACP;

        // -------------------------------------------------------------------------
        // ! if adding new shader models add them to cache config in ShaderManager !
        // -------------------------------------------------------------------------
        /** Used vertex shader model. */
        static inline std::string sVertexShaderModel = "vs_6_0";
        /** Used pixel shader model. */
        static inline std::string sPixelShaderModel = "ps_6_0";
        /** Used compute shader model. */
        static inline std::string sComputeShaderModel = "cs_6_0";
        // -------------------------------------------------------------------------
        // ! if adding new shader models add them to cache config in ShaderManager !
        // -------------------------------------------------------------------------
    };
} // namespace ne
