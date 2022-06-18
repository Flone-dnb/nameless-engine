// Custom.
#include "shaders/IShader.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#include "shaders/hlsl/HlslShader.h"
#endif

// STL.
#include <fstream>

// External.
#include "Catch2/catch_test_macros.hpp"

constexpr auto sTopLevelShaderName = "test_shader";

#if defined(WIN32)
TEST_CASE("compile HLSL vertex shader") {
    using namespace ne;

    auto shaderPath = std::filesystem::temp_directory_path() / sTopLevelShaderName;
    shaderPath += ".hlsl";

    // Create a simple shader file.
    std::ofstream shaderFile(shaderPath);
    REQUIRE(shaderFile.is_open());
    shaderFile << "float4 vs(float3 vPos : POSITION) : SV_POSITION\n"
                  "{\n"
                  "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                  "}\n";
    shaderFile.close();

    ShaderDescription description{"test shader", shaderPath, ShaderType::VERTEX_SHADER, "vs", {}};
    std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
    auto result = IShader::compileShader<HlslShader>(description, cacheInvalidationReason);

    if (!std::holds_alternative<std::shared_ptr<IShader>>(result)) {
        std::string sErrorMessage;
        if (std::holds_alternative<std::string>(result)) {
            sErrorMessage = std::get<std::string>(result);
        } else {
            sErrorMessage = std::get<Error>(result).getError();
        }
        INFO(sErrorMessage);
        REQUIRE(std::holds_alternative<std::shared_ptr<IShader>>(result));
    }

    // Cleanup.
    std::filesystem::remove(shaderPath);
}
#endif

#if defined(WIN32)
TEST_CASE("compile HLSL pixel shader") {
    using namespace ne;

    auto shaderPath = std::filesystem::temp_directory_path() / sTopLevelShaderName;
    shaderPath += ".hlsl";

    // Create a simple shader file.
    std::ofstream shaderFile(shaderPath);
    REQUIRE(shaderFile.is_open());
    shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                  "{\n"
                  "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                  "}\n";
    shaderFile.close();

    ShaderDescription description{"test shader", shaderPath, ShaderType::PIXEL_SHADER, "ps", {}};
    std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
    auto result = IShader::compileShader<HlslShader>(description, cacheInvalidationReason);

    if (!std::holds_alternative<std::shared_ptr<IShader>>(result)) {
        std::string sErrorMessage;
        if (std::holds_alternative<std::string>(result)) {
            sErrorMessage = std::get<std::string>(result);
        } else {
            sErrorMessage = std::get<Error>(result).getError();
        }
        INFO(sErrorMessage);
        REQUIRE(std::holds_alternative<std::shared_ptr<IShader>>(result));
    }

    // Cleanup.
    std::filesystem::remove(shaderPath);
}
#endif

#if defined(WIN32)
TEST_CASE("compile HLSL compute shader") {
    using namespace ne;

    auto shaderPath = std::filesystem::temp_directory_path() / sTopLevelShaderName;
    shaderPath += ".hlsl";

    // Create a simple shader file.
    std::ofstream shaderFile(shaderPath);
    REQUIRE(shaderFile.is_open());
    shaderFile << "[numthreads(1, 1, 1)]\n"
                  "void cs(){}\n";
    shaderFile.close();

    ShaderDescription description{"test shader", shaderPath, ShaderType::COMPUTE_SHADER, "cs", {}};
    std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
    auto result = IShader::compileShader<HlslShader>(description, cacheInvalidationReason);

    if (!std::holds_alternative<std::shared_ptr<IShader>>(result)) {
        std::string sErrorMessage;
        if (std::holds_alternative<std::string>(result)) {
            sErrorMessage = std::get<std::string>(result);
        } else {
            sErrorMessage = std::get<Error>(result).getError();
        }
        INFO(sErrorMessage);
        REQUIRE(std::holds_alternative<std::shared_ptr<IShader>>(result));
    }

    // Cleanup.
    std::filesystem::remove(shaderPath);
}
#endif