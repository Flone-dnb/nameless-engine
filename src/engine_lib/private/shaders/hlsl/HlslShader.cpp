#include "shaders/hlsl/HlslShader.h"

namespace ne {
    HlslShader::HlslShader(std::filesystem::path pathToCompiledShader)
        : IShader(std::move(pathToCompiledShader)) {}

    std::variant<std::unique_ptr<IShader>, std::string, Error>
    HlslShader::compileShader(ShaderDescription shaderDescription) {
        // TODO
        return std::string();
    }

    ComPtr<IDxcBlob> HlslShader::getCompiledBlob() {
        if (!pCompiledBlob) {
            auto pathToCompiledShader = getPathToCompiledShader();
            // TODO: load from disk
        }

        return pCompiledBlob;
    }
} // namespace ne
