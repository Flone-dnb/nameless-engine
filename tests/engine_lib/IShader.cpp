// Custom.
#include "shaders/IShader.h"
#include "game/IGameInstance.h"
#include "game/Window.h"
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

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
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
            auto result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
#endif

#if defined(WIN32)
TEST_CASE("compile HLSL pixel shader") {
    using namespace ne;

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
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
            auto result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
#endif

#if defined(WIN32)
TEST_CASE("compile HLSL compute shader") {
    using namespace ne;

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
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
            auto result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
#endif

#if defined(WIN32)
TEST_CASE("find valid HLSL shader cache") {
    using namespace ne;

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
            auto shaderPath = std::filesystem::temp_directory_path() / sTopLevelShaderName;
            shaderPath += ".hlsl";

            // Create a simple shader file (initial file).
            std::ofstream shaderFile(shaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                          "{\n"
                          "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                          "}\n";
            shaderFile.close();

            ShaderDescription description{"test shader", shaderPath, ShaderType::PIXEL_SHADER, "ps", {}};
            std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
            auto result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            // Compile again (should find valid cache).
            result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            REQUIRE(!cacheInvalidationReason.has_value());

            // Cleanup.
            std::filesystem::remove(shaderPath);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
#endif

#if defined(WIN32)
TEST_CASE("invalidate HLSL shader cache - ENTRY_FUNCTION_NAME_CHANGED") {
    using namespace ne;

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
            auto shaderPath = std::filesystem::temp_directory_path() / sTopLevelShaderName;
            shaderPath += ".hlsl";

            // Create a simple shader file (initial file).
            std::ofstream shaderFile(shaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                          "{\n"
                          "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                          "}\n";
            shaderFile.close();

            ShaderDescription description{"test shader", shaderPath, ShaderType::PIXEL_SHADER, "ps", {}};
            std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
            auto result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            // Compile again (should invalidate cache).
            // Overwrite initial file (change entry function name).
            shaderFile.open(shaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "float4 pss(float4 vPos : SV_POSITION) : SV_Target\n"
                          "{\n"
                          "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                          "}\n";
            shaderFile.close();
            description.sShaderEntryFunctionName = "pss";
            result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            REQUIRE(cacheInvalidationReason.has_value());
            REQUIRE(
                cacheInvalidationReason.value() ==
                ShaderCacheInvalidationReason::ENTRY_FUNCTION_NAME_CHANGED);

            // Cleanup.
            std::filesystem::remove(shaderPath);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
#endif

#if defined(WIN32)
TEST_CASE("invalidate HLSL shader cache - SHADER_TYPE_CHANGED") {
    using namespace ne;

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
            auto shaderPath = std::filesystem::temp_directory_path() / sTopLevelShaderName;
            shaderPath += ".hlsl";

            // Create a simple shader file (initial file).
            std::ofstream shaderFile(shaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                          "{\n"
                          "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                          "}\n";
            shaderFile.close();

            ShaderDescription description{"test shader", shaderPath, ShaderType::PIXEL_SHADER, "ps", {}};
            std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
            auto result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            // Compile again (should invalidate cache).
            // Overwrite initial file (change shader type).
            shaderFile.open(shaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "float4 ps(float3 vPos : POSITION) : SV_POSITION\n" // keep old entry function name
                          "{\n"
                          "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                          "}\n";
            shaderFile.close();
            description.shaderType = ShaderType::VERTEX_SHADER;
            result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            REQUIRE(cacheInvalidationReason.has_value());
            REQUIRE(cacheInvalidationReason.value() == ShaderCacheInvalidationReason::SHADER_TYPE_CHANGED);

            // Cleanup.
            std::filesystem::remove(shaderPath);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
#endif

#if defined(WIN32)
TEST_CASE("invalidate HLSL shader cache - DEFINED_SHADER_MACROS_CHANGED") {
    using namespace ne;

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
            auto shaderPath = std::filesystem::temp_directory_path() / sTopLevelShaderName;
            shaderPath += ".hlsl";

            // Create a simple shader file (initial file).
            std::ofstream shaderFile(shaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                          "{\n"
                          "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                          "}\n";
            shaderFile.close();

            ShaderDescription description{"test shader", shaderPath, ShaderType::PIXEL_SHADER, "ps", {}};
            std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
            auto result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            // Compile again (should invalidate cache).
            // Add some defines.
            description.vDefinedShaderMacros = {"test1", "test2"};
            result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            REQUIRE(cacheInvalidationReason.has_value());
            REQUIRE(
                cacheInvalidationReason.value() ==
                ShaderCacheInvalidationReason::DEFINED_SHADER_MACROS_CHANGED);

            // Compile again (should find valid cache).
            // Reorder defines.
            description.vDefinedShaderMacros = {"test2", "test1"};
            result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            REQUIRE(!cacheInvalidationReason.has_value());

            // Cleanup.
            std::filesystem::remove(shaderPath);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
#endif

#if defined(WIN32)
TEST_CASE("invalidate HLSL shader cache - SHADER_SOURCE_FILE_CHANGED") {
    using namespace ne;

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
            auto shaderPath = std::filesystem::temp_directory_path() / sTopLevelShaderName;
            shaderPath += ".hlsl";

            // Create a simple shader file (initial file).
            std::ofstream shaderFile(shaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                          "{\n"
                          "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                          "}\n";
            shaderFile.close();

            ShaderDescription description{"test shader", shaderPath, ShaderType::PIXEL_SHADER, "ps", {}};
            std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
            auto result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            // Compile again (should invalidate cache).
            // Change source code.
            shaderFile.open(shaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                          "{\n"
                          "return float4(0.0f, 1.0f, 1.0f, 1.0f);\n"
                          "}\n";
            shaderFile.close();
            result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            REQUIRE(cacheInvalidationReason.has_value());
            REQUIRE(
                cacheInvalidationReason.value() == ShaderCacheInvalidationReason::SHADER_SOURCE_FILE_CHANGED);

            // Cleanup.
            std::filesystem::remove(shaderPath);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
#endif

#if defined(WIN32)
TEST_CASE("invalidate HLSL shader cache - SHADER_INCLUDE_TREE_CONTENT_CHANGED") {
    using namespace ne;

    class TestGameInstance : public IGameInstance {
    public:
        TestGameInstance(Window* pGameWindow, InputManager* pInputManager)
            : IGameInstance(pGameWindow, pInputManager) {
            auto shaderPath = std::filesystem::temp_directory_path() / sTopLevelShaderName;
            shaderPath += ".hlsl";

            // Create the following shader tree:
            // shader.hlsl
            //   ^--- [includes] foo.hlsl, test_shaders/bar.hlsl
            //                                           ^--- [includes] foo.hlsl (another foo.hlsl).

            // Top level shader.
            std::ofstream shaderFile(shaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "#include \"test_shaders/bar.hlsl\"\n"
                          "#include \"foo.hlsl\"\n"
                          "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                          "{\n"
                          "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                          "}\n";
            shaderFile.close();

            // foo.hlsl
            const auto fooShaderPath = std::filesystem::temp_directory_path() / "foo.hlsl";
            shaderFile.open(fooShaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "void foo(){};\n";
            shaderFile.close();

            const auto testShadersDirPath = std::filesystem::temp_directory_path() / "test_shaders";
            std::filesystem::create_directory(testShadersDirPath);

            // test_shaders/bar.hlsl
            const auto barShaderPath = testShadersDirPath / "bar.hlsl";
            shaderFile.open(barShaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "#include \"foo.hlsl\"\n"
                          "void bar(){};\n";
            shaderFile.close();

            // test_shaders/foo.hlsl
            const auto anotherFooShaderPath = testShadersDirPath / "foo.hlsl";
            shaderFile.open(anotherFooShaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "void foo2(){};\n";
            shaderFile.close();

            ShaderDescription description{"test shader", shaderPath, ShaderType::PIXEL_SHADER, "ps", {}};
            std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
            auto result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            // Compile again (should find valid cache).
            result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            REQUIRE(!cacheInvalidationReason.has_value());

            // Change test_shaders/foo.hlsl source code.
            shaderFile.open(anotherFooShaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "void foo2(){ };\n";
            shaderFile.close();
            result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            REQUIRE(cacheInvalidationReason.has_value());
            REQUIRE(
                cacheInvalidationReason.value() ==
                ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED);

            // Change bar.hlsl source code.
            shaderFile.open(barShaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "#include \"foo.hlsl\"\n"
                          "void bar(){ };\n";
            shaderFile.close();
            result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            REQUIRE(cacheInvalidationReason.has_value());
            REQUIRE(
                cacheInvalidationReason.value() ==
                ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED);

            // Change bar.hlsl source code (remove include).
            shaderFile.open(barShaderPath);
            REQUIRE(shaderFile.is_open());
            shaderFile << "void bar(){ };\n";
            shaderFile.close();
            result = IShader::compileShader<HlslShader>(
                description, pGameWindow->getRenderer(), cacheInvalidationReason);

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

            REQUIRE(cacheInvalidationReason.has_value());
            REQUIRE(
                cacheInvalidationReason.value() ==
                ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED);

            // Cleanup.
            std::filesystem::remove(shaderPath);
            std::filesystem::remove_all(testShadersDirPath);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addEntry();
        INFO(error.getError());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
#endif