﻿// Standard.
#include <fstream>

// Custom.
#include "shader/general/ShaderPack.h"
#include "game/GameInstance.h"
#include "game/Window.h"
#include "misc/ProjectPaths.h"
#if defined(WIN32)
#include "render/directx/DirectXRenderer.h"
#endif
#include "render/vulkan/VulkanRenderer.h"
#include "shader/general/ShaderFilesystemPaths.hpp"

// External.
#include "catch2/catch_test_macros.hpp"

const inline std::filesystem::path shaderPathNoExtension =
    ne::ProjectPaths::getPathToResDirectory(ne::ResourceDirectory::ROOT) / "test" / "temp" / "test_shader";

constexpr std::string_view sSampleHlslVertexShader = "float4 vs(float3 pos : POSITION) : SV_POSITION\n"
                                                     "{\n"
                                                     "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                                                     "}\n";
constexpr std::string_view sSampleHlslPixelShader = "float4 ps(float4 pos : SV_POSITION) : SV_Target\n"
                                                    "{\n"
                                                    "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                                                    "}\n";
constexpr std::string_view sSampleHlslComputeShader = "[numthreads(1, 1, 1)]\n"
                                                      "void cs(){}\n";

constexpr std::string_view sSampleGlslVertexShader = "#version 450\n"
                                                     "layout(location = 0) in vec3 position;\n"
                                                     "void main(){\n"
                                                     "gl_Position = vec4(position, 1.0F);\n"
                                                     "}\n";
constexpr std::string_view sSampleGlslPixelShader = "#version 450\n"
                                                    "layout(location = 0) out vec4 outColor;\n"
                                                    "void main(){\n"
                                                    "outColor = vec4(1.0F, 1.0F, 1.0F, 1.0F);\n"
                                                    "}\n";
constexpr std::string_view sSampleGlslComputeShader = "#version 450\n"
                                                      "layout (local_size_x = 128) in;\n"
                                                      "shared float foobar [128];\n"
                                                      "void main(){\n"
                                                      "foobar [gl_LocalInvocationIndex] = 0.0;\n"
                                                      "}\n";

const inline std::string sSampleHlslVertexShaderEntryName = "vs";
const inline std::string sSampleHlslPixelShaderEntryName = "ps";
const inline std::string sSampleHlslComputeShaderEntryName = "cs";

const inline std::string sSampleShaderName = "test shader";

std::variant<std::shared_ptr<ne::ShaderPack>, std::string, ne::Error>
createSampleVertexShader(ne::Renderer* pRenderer, ne::ShaderDescription& outDescription) {
#if defined(WIN32)
    if (dynamic_cast<ne::DirectXRenderer*>(pRenderer) != nullptr) {
        std::ofstream shaderFile(shaderPathNoExtension.string() + ".hlsl");
        REQUIRE(shaderFile.is_open());
        shaderFile << sSampleHlslVertexShader;
        shaderFile.close();

        // Compile the shader.
        outDescription = ne::ShaderDescription{
            sSampleShaderName,
            shaderPathNoExtension.string() + ".hlsl",
            ne::ShaderType::VERTEX_SHADER,
            {},
            sSampleHlslVertexShaderEntryName,
            {}};
        return ne::ShaderPack::compileShaderPack(pRenderer, outDescription);
    }
#endif
    if (dynamic_cast<ne::VulkanRenderer*>(pRenderer) != nullptr) {
        std::ofstream shaderFile(shaderPathNoExtension.string() + ".glsl");
        REQUIRE(shaderFile.is_open());
        shaderFile << sSampleGlslVertexShader;
        shaderFile.close();

        // Compile the shader.
        outDescription = ne::ShaderDescription{
            sSampleShaderName,
            shaderPathNoExtension.string() + ".glsl",
            ne::ShaderType::VERTEX_SHADER,
            {},
            "main",
            {}};
        return ne::ShaderPack::compileShaderPack(pRenderer, outDescription);
    }
    INFO("unsupported renderer");
    REQUIRE(false);

    return ne::Error("unsupported renderer");
}

std::variant<std::shared_ptr<ne::ShaderPack>, std::string, ne::Error>
createSamplePixelShader(ne::Renderer* pRenderer, ne::ShaderDescription& outDescription) {
#if defined(WIN32)
    if (dynamic_cast<ne::DirectXRenderer*>(pRenderer) != nullptr) {
        std::ofstream shaderFile(shaderPathNoExtension.string() + ".hlsl");
        REQUIRE(shaderFile.is_open());
        shaderFile << sSampleHlslPixelShader;
        shaderFile.close();

        // Compile the shader.
        outDescription = ne::ShaderDescription{
            sSampleShaderName,
            shaderPathNoExtension.string() + ".hlsl",
            ne::ShaderType::FRAGMENT_SHADER,
            {},
            sSampleHlslPixelShaderEntryName,
            {}};
        return ne::ShaderPack::compileShaderPack(pRenderer, outDescription);
    }
#endif
    if (dynamic_cast<ne::VulkanRenderer*>(pRenderer) != nullptr) {
        std::ofstream shaderFile(shaderPathNoExtension.string() + ".glsl");
        REQUIRE(shaderFile.is_open());
        shaderFile << sSampleGlslPixelShader;
        shaderFile.close();

        // Compile the shader.
        outDescription = ne::ShaderDescription{
            sSampleShaderName,
            shaderPathNoExtension.string() + ".glsl",
            ne::ShaderType::FRAGMENT_SHADER,
            {},
            "main",
            {}};
        return ne::ShaderPack::compileShaderPack(pRenderer, outDescription);
    }
    INFO("unsupported renderer");
    REQUIRE(false);

    return ne::Error("unsupported renderer");
}

std::variant<std::shared_ptr<ne::ShaderPack>, std::string, ne::Error>
createSampleComputeShader(ne::Renderer* pRenderer, ne::ShaderDescription& outDescription) {
#if defined(WIN32)
    if (dynamic_cast<ne::DirectXRenderer*>(pRenderer) != nullptr) {
        std::ofstream shaderFile(shaderPathNoExtension.string() + ".hlsl");
        REQUIRE(shaderFile.is_open());
        shaderFile << sSampleHlslComputeShader;
        shaderFile.close();

        // Compile the shader.
        outDescription = ne::ShaderDescription{
            sSampleShaderName,
            shaderPathNoExtension.string() + ".hlsl",
            ne::ShaderType::COMPUTE_SHADER,
            {},
            sSampleHlslComputeShaderEntryName,
            {}};
        return ne::ShaderPack::compileShaderPack(pRenderer, outDescription);
    }
#endif
    if (dynamic_cast<ne::VulkanRenderer*>(pRenderer) != nullptr) {
        std::ofstream shaderFile(shaderPathNoExtension.string() + ".glsl");
        REQUIRE(shaderFile.is_open());
        shaderFile << sSampleGlslComputeShader;
        shaderFile.close();

        // Compile the shader.
        outDescription = ne::ShaderDescription{
            sSampleShaderName,
            shaderPathNoExtension.string() + ".glsl",
            ne::ShaderType::COMPUTE_SHADER,
            {},
            "main",
            {}};
        return ne::ShaderPack::compileShaderPack(pRenderer, outDescription);
    }

    INFO("unsupported renderer");
    REQUIRE(false);

    return ne::Error("unsupported renderer");
}

TEST_CASE("compile a vertex shader") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Create shader.
            ShaderDescription desc;
            auto result = createSampleVertexShader(pGameWindow->getRenderer(), desc);

            // Check results.
            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(result)) {
                std::string sErrorMessage;
                if (std::holds_alternative<std::string>(result)) {
                    sErrorMessage = std::get<std::string>(result);
                } else {
                    sErrorMessage = std::get<Error>(result).getFullErrorMessage();
                }
                INFO(sErrorMessage);
                REQUIRE(false);
            }

            // Cleanup.
            std::filesystem::remove(desc.pathToShaderFile);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const auto pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("compile a pixel shader") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Create shader.
            ShaderDescription desc;
            auto result = createSamplePixelShader(pGameWindow->getRenderer(), desc);

            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(result)) {
                std::string sErrorMessage;
                if (std::holds_alternative<std::string>(result)) {
                    sErrorMessage = std::get<std::string>(result);
                } else {
                    sErrorMessage = std::get<Error>(result).getFullErrorMessage();
                }
                INFO(sErrorMessage);
                REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(result));
            }

            // Cleanup.
            std::filesystem::remove(desc.pathToShaderFile);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const auto pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("compile a compute shader") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Create shader.
            ShaderDescription desc;
            auto result = createSampleComputeShader(pGameWindow->getRenderer(), desc);

            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(result)) {
                std::string sErrorMessage;
                if (std::holds_alternative<std::string>(result)) {
                    sErrorMessage = std::get<std::string>(result);
                } else {
                    sErrorMessage = std::get<Error>(result).getFullErrorMessage();
                }
                INFO(sErrorMessage);
                REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(result));
            }

            // Cleanup.
            std::filesystem::remove(desc.pathToShaderFile);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const auto pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("find valid shader cache") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Create shader.
            ShaderDescription desc;
            auto compileResult = createSamplePixelShader(pGameWindow->getRenderer(), desc);

            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                std::string sErrorMessage;
                if (std::holds_alternative<std::string>(compileResult)) {
                    sErrorMessage = std::get<std::string>(compileResult);
                } else {
                    sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                }
                INFO(sErrorMessage);
                REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
            }

            // Use cache.
            std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;

            auto result =
                ShaderPack::createFromCache(pGameWindow->getRenderer(), desc, cacheInvalidationReason);

            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(result)) {
                std::string sErrorMessage = std::get<Error>(result).getFullErrorMessage();
                INFO(sErrorMessage);
                REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(result));
            }

            REQUIRE(!cacheInvalidationReason.has_value());

            // Cleanup.
            std::filesystem::remove(desc.pathToShaderFile);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const auto pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("invalidate shader cache - ENTRY_FUNCTION_NAME_CHANGED") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
#if defined(WIN32)
            if (dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
                shaderPath += ".hlsl";

                // Create a simple shader file (initial file).
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                              "{\n"
                              "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                              "}\n";
                shaderFile.close();

                ShaderDescription description{
                    sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "ps", {}};
                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Overwrite initial file (change entry function name).
                shaderFile.open(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "float4 pss(float4 vPos : SV_POSITION) : SV_Target\n"
                              "{\n"
                              "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                              "}\n";
                shaderFile.close();
                description.sShaderEntryFunctionName = "pss";

                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;

                // Should invalidate cache.
                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::ENTRY_FUNCTION_NAME_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);

                pGameWindow->close();
                return;
            }
#endif

            if (dynamic_cast<VulkanRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
                shaderPath += ".glsl";

                // Create a simple shader file (initial file).
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#version 450\n"
                              "layout(location = 0) out vec4 outColor;\n"
                              "void main(){\n"
                              "outColor = vec4(1.0F, 1.0F, 1.0F, 1.0F);\n"
                              "}\n";
                shaderFile.close();

                ShaderDescription description{
                    sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "main", {}};
                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Overwrite initial file (change entry function name).
                shaderFile.open(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#version 450\n"
                              "layout(location = 0) out vec4 outColor;\n"
                              "void pss(){\n"
                              "outColor = vec4(1.0F, 1.0F, 1.0F, 1.0F);\n"
                              "}\n";
                shaderFile.close();
                description.sShaderEntryFunctionName = "pss";

                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;

                // Should invalidate cache.
                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::ENTRY_FUNCTION_NAME_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);

                pGameWindow->close();
                return;
            }

            INFO("unsupported renderer");
            REQUIRE(false);
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("invalidate shader cache - SHADER_TYPE_CHANGED") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
#if defined(WIN32)
            if (dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
                shaderPath += ".hlsl";

                // Create a simple shader file (initial file).
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                              "{\n"
                              "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                              "}\n";
                shaderFile.close();

                ShaderDescription description{
                    sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "ps", {}};
                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Overwrite initial file (change shader type).
                shaderFile.open(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile
                    << "float4 ps(float3 vPos : POSITION) : SV_POSITION\n" // keep old entry function name
                       "{\n"
                       "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                       "}\n";
                shaderFile.close();
                description.shaderType = ShaderType::VERTEX_SHADER;

                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;

                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                // REQUIRE(cacheInvalidationReason.has_value()); // parameters might be different
                // REQUIRE(cacheInvalidationReason.value() ==
                // ShaderCacheInvalidationReason::SHADER_TYPE_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);

                pGameWindow->close();
                return;
            }
#endif

            if (dynamic_cast<VulkanRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
                shaderPath += ".glsl";

                // Create a simple shader file (initial file).
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#version 450\n"
                              "layout(location = 0) out vec4 outColor;\n"
                              "void main(){\n"
                              "outColor = vec4(1.0F, 1.0F, 1.0F, 1.0F);\n"
                              "}\n";
                shaderFile.close();

                ShaderDescription description{
                    sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "main", {}};
                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Overwrite initial file (change shader type).
                shaderFile.open(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#version 450\n"
                              "layout(location = 0) in vec3 position;\n"
                              "void main(){\n" // keep old entry function name
                              "gl_Position = vec4(position, 1.0F);\n"
                              "}\n";
                shaderFile.close();
                description.shaderType = ShaderType::VERTEX_SHADER;

                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;

                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                // REQUIRE(cacheInvalidationReason.has_value()); // parameters might be different
                // REQUIRE(cacheInvalidationReason.value() ==
                // ShaderCacheInvalidationReason::SHADER_TYPE_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);

                pGameWindow->close();
                return;
            }

            INFO("unsupported renderer");
            REQUIRE(false);
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("invalidate shader cache - DEFINED_SHADER_MACROS_CHANGED") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
            // Prepare shader description.
            ShaderDescription description;

            if (dynamic_cast<VulkanRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
                shaderPath += ".glsl";

                // Create a simple shader file (initial file).
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#version 450\n"
                              "layout(location = 0) out vec4 outColor;\n"
                              "void main(){\n"
                              "outColor = vec4(1.0F, 1.0F, 1.0F, 1.0F);\n"
                              "}\n";
                shaderFile.close();

                description = ShaderDescription(
                    sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "main", {});
            } else {
#if defined(WIN32)
                if (dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                    auto shaderPath = shaderPathNoExtension;
                    shaderPath += ".hlsl";

                    // Create a simple shader file (initial file).
                    std::ofstream shaderFile(shaderPath);
                    REQUIRE(shaderFile.is_open());
                    shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                                  "{\n"
                                  "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                                  "}\n";
                    shaderFile.close();

                    description = ShaderDescription(
                        sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "ps", {});
                }
#endif
            }

            auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                std::string sErrorMessage;
                if (std::holds_alternative<std::string>(compileResult)) {
                    sErrorMessage = std::get<std::string>(compileResult);
                } else {
                    sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                }
                INFO(sErrorMessage);
                REQUIRE(false);
            }

            // Add some defines.
            description.definedShaderMacros = {{"test1", "value1"}, {"test2", ""}};

            std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;

            auto cacheResult =
                ShaderPack::createFromCache(pGameWindow->getRenderer(), description, cacheInvalidationReason);

            // Clears shader cache due to invalidation.
            REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
            REQUIRE(cacheInvalidationReason.has_value());
            REQUIRE(
                cacheInvalidationReason.value() ==
                ShaderCacheInvalidationReason::DEFINED_SHADER_MACROS_CHANGED);

            // Compile new version.
            compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                std::string sErrorMessage;
                if (std::holds_alternative<std::string>(compileResult)) {
                    sErrorMessage = std::get<std::string>(compileResult);
                } else {
                    sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                }
                INFO(sErrorMessage);
                REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
            }

            // Reorder defines (should be OK to use cache).
            description.definedShaderMacros = {{"test2", ""}, {"test1", "value1"}};
            cacheResult =
                ShaderPack::createFromCache(pGameWindow->getRenderer(), description, cacheInvalidationReason);

            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult)) {
                std::string sErrorMessage = std::get<Error>(cacheResult).getFullErrorMessage();
                INFO(sErrorMessage);
                REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
            }
            REQUIRE(!cacheInvalidationReason.has_value());

            // Change value (should invalidate cache).
            description.definedShaderMacros = {{"test2", ""}, {"test1", "value2"}};
            cacheResult =
                ShaderPack::createFromCache(pGameWindow->getRenderer(), description, cacheInvalidationReason);

            // Clears shader cache due to invalidation.
            REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
            REQUIRE(cacheInvalidationReason.has_value());
            REQUIRE(
                cacheInvalidationReason.value() ==
                ShaderCacheInvalidationReason::DEFINED_SHADER_MACROS_CHANGED);

            // Compile new version.
            compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
            if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                std::string sErrorMessage;
                if (std::holds_alternative<std::string>(compileResult)) {
                    sErrorMessage = std::get<std::string>(compileResult);
                } else {
                    sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                }
                INFO(sErrorMessage);
                REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
            }

            // Add value to existing macro (should invalidate cache).
            description.definedShaderMacros = {{"test2", "new"}, {"test1", "value2"}};
            cacheResult =
                ShaderPack::createFromCache(pGameWindow->getRenderer(), description, cacheInvalidationReason);

            REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
            REQUIRE(cacheInvalidationReason.has_value());
            REQUIRE(
                cacheInvalidationReason.value() ==
                ShaderCacheInvalidationReason::DEFINED_SHADER_MACROS_CHANGED);

            // Cleanup.
            std::filesystem::remove(description.pathToShaderFile);

            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("invalidate shader cache - SHADER_SOURCE_FILE_CHANGED") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
#if defined(WIN32)
            if (dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
                shaderPath += ".hlsl";

                // Create a simple shader file (initial file).
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                              "{\n"
                              "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                              "}\n";
                shaderFile.close();

                ShaderDescription description{
                    sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "ps", {}};

                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Change source code.
                shaderFile.open(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                              "{\n"
                              "return float4(0.0f, 1.0f, 1.0f, 1.0f);\n"
                              "}\n";
                shaderFile.close();

                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;

                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::SHADER_SOURCE_FILE_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);

                pGameWindow->close();
                return;
            }
#endif

            if (dynamic_cast<VulkanRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
                shaderPath += ".glsl";

                // Create a simple shader file (initial file).
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#version 450\n"
                              "layout(location = 0) out vec4 outColor;\n"
                              "void main(){\n"
                              "outColor = vec4(1.0F, 1.0F, 1.0F, 1.0F);\n"
                              "}\n";
                shaderFile.close();

                ShaderDescription description{
                    sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "main", {}};

                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Change source code.
                shaderFile.open(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#version 450\n"
                              "layout(location = 0) out vec4 outColor;\n"
                              "void  main(){\n"
                              "outColor = vec4(1.0F, 1.0F, 1.0F, 1.0F);\n"
                              "}\n";
                shaderFile.close();

                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;

                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::SHADER_SOURCE_FILE_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);

                pGameWindow->close();
                return;
            }

            INFO("unsupported renderer");
            REQUIRE(false);
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("invalidate HLSL shader cache - SHADER_INCLUDE_TREE_CONTENT_CHANGED") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
#if defined(WIN32)
            if (dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
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
                const auto fooShaderPath = ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                                           "test" / "temp" / "foo.hlsl";
                shaderFile.open(fooShaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "void foo(){};\n";
                shaderFile.close();

                const auto testShadersDirPath = ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                                                "test" / "temp" / "test_shaders";
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

                ShaderDescription description{
                    "test shader", shaderPath, ShaderType::FRAGMENT_SHADER, {}, "ps", {}};

                // Compile initial version.
                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Should find valid cache.
                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult)) {
                    std::string sErrorMessage = std::get<Error>(cacheResult).getFullErrorMessage();
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                }

                REQUIRE(!cacheInvalidationReason.has_value());

                // Change test_shaders/foo.hlsl source code.
                shaderFile.open(anotherFooShaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "void foo2(){ };\n";
                shaderFile.close();

                // Cache should be invalidated.
                cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED);

                // Compile new version.
                compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Change bar.hlsl source code.
                shaderFile.open(barShaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#include \"foo.hlsl\"\n"
                              "void bar(){ };\n";
                shaderFile.close();

                // Cache should be invalidated.
                cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED);

                // Compile new version.
                compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Change bar.hlsl source code (remove include).
                shaderFile.open(barShaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "void bar(){ };\n";
                shaderFile.close();

                // Cache should be invalidated.
                cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);
                std::filesystem::remove_all(testShadersDirPath);

                pGameWindow->close();
                return;
            }
#endif

            if (dynamic_cast<VulkanRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
                shaderPath += ".glsl";

                // Create the following shader tree:
                // shader.glsl
                //   ^--- [includes] foo.glsl, test_shaders/bar.glsl
                //                                           ^--- [includes] foo.glsl (another foo.glsl).

                // Top level shader.
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#version 450\n"
                              "#include \"test_shaders/bar.glsl\"\n"
                              "#include \"foo.glsl\"\n"
                              "layout(location = 0) out vec4 outColor;\n"
                              "void main(){\n"
                              "outColor = vec4(1.0F, 1.0F, 1.0F, 1.0F);\n"
                              "}\n";
                shaderFile.close();

                // foo.glsl
                const auto fooShaderPath = ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                                           "test" / "temp" / "foo.glsl";
                shaderFile.open(fooShaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "void foo(){}\n";
                shaderFile.close();

                const auto testShadersDirPath = ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                                                "test" / "temp" / "test_shaders";
                std::filesystem::create_directory(testShadersDirPath);

                // test_shaders/bar.glsl
                const auto barShaderPath = testShadersDirPath / "bar.glsl";
                shaderFile.open(barShaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#include \"foo.glsl\"\n"
                              "void bar(){}\n";
                shaderFile.close();

                // test_shaders/foo.glsl
                const auto anotherFooShaderPath = testShadersDirPath / "foo.glsl";
                shaderFile.open(anotherFooShaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "void foo2(){}\n";
                shaderFile.close();

                ShaderDescription description{
                    sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "main", {}};

                // Compile initial version.
                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Should find valid cache.
                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult)) {
                    std::string sErrorMessage = std::get<Error>(cacheResult).getFullErrorMessage();
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                }

                REQUIRE(!cacheInvalidationReason.has_value());

                // Change test_shaders/foo.glsl source code.
                shaderFile.open(anotherFooShaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "void foo2(){ }\n";
                shaderFile.close();

                // Cache should be invalidated.
                cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED);

                // Compile new version.
                compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Change bar.glsl source code.
                shaderFile.open(barShaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#include \"foo.glsl\"\n"
                              "void bar(){ }\n";
                shaderFile.close();

                // Cache should be invalidated.
                cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED);

                // Compile new version.
                compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Change bar.glsl source code (remove include).
                shaderFile.open(barShaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "void bar(){ }\n";
                shaderFile.close();

                // Cache should be invalidated.
                cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::SHADER_INCLUDE_TREE_CONTENT_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);
                std::filesystem::remove_all(testShadersDirPath);

                pGameWindow->close();
                return;
            }

            INFO("unsupported renderer");
            REQUIRE(false);
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("invalidate shader cache - COMPILED_BINARY_CHANGED (bytecode)") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
#if defined(WIN32)
            if (dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
                shaderPath += ".hlsl";

                // Create temporary shader file.
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                              "{\n"
                              "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                              "}\n";
                shaderFile.close();

                ShaderDescription description{
                    sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "ps", {}};

                // Compile.
                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Should find valid cache.
                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult)) {
                    std::string sErrorMessage = std::get<Error>(cacheResult).getFullErrorMessage();
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                }
                REQUIRE(!cacheInvalidationReason.has_value());
                const auto pShaderPack = std::get<std::shared_ptr<ShaderPack>>(cacheResult);

                // Get path to shader bytecode.
                const auto pathToShaderBytecode = ProjectPaths::getPathToCompiledShadersDirectory() /
                                                  sSampleShaderName /
                                                  ShaderFilesystemPaths::getShaderCacheBaseFileName();
                REQUIRE(std::filesystem::exists(pathToShaderBytecode));

                // Now manually change shader bytecode.
                // It's enough to modify bytecode of just one shader configuration for cache to be invalid.
                std::ofstream shaderBytecodeFile(pathToShaderBytecode);
                REQUIRE(shaderBytecodeFile.is_open());
                shaderBytecodeFile << "Hello World!";
                shaderBytecodeFile.close();

                // Cache should be invalidated.
                cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::COMPILED_BINARY_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);

                pGameWindow->close();
                return;
            }
#endif

            if (dynamic_cast<VulkanRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                auto shaderPath = shaderPathNoExtension;
                shaderPath += ".glsl";

                // Create temporary shader file.
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "#version 450\n"
                              "layout(location = 0) out vec4 outColor;\n"
                              "void main(){\n"
                              "outColor = vec4(1.0F, 1.0F, 1.0F, 1.0F);\n"
                              "}\n";
                shaderFile.close();

                ShaderDescription description{
                    sSampleShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "main", {}};

                // Compile.
                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Should find valid cache.
                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult)) {
                    std::string sErrorMessage = std::get<Error>(cacheResult).getFullErrorMessage();
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                }
                REQUIRE(!cacheInvalidationReason.has_value());
                const auto pShaderPack = std::get<std::shared_ptr<ShaderPack>>(cacheResult);

                // Get path to shader bytecode.
                const auto pathToShaderBytecode = ProjectPaths::getPathToCompiledShadersDirectory() /
                                                  sSampleShaderName /
                                                  ShaderFilesystemPaths::getShaderCacheBaseFileName();
                REQUIRE(std::filesystem::exists(pathToShaderBytecode));

                // Now manually change shader bytecode.
                // It's enough to modify bytecode of just one shader configuration for cache to be invalid.
                std::ofstream shaderBytecodeFile(pathToShaderBytecode);
                REQUIRE(shaderBytecodeFile.is_open());
                shaderBytecodeFile << "Hello World!";
                shaderBytecodeFile.close();

                // Cache should be invalidated.
                cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::COMPILED_BINARY_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);

                pGameWindow->close();
                return;
            }

            INFO("unsupported renderer");
            REQUIRE(false);
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}

TEST_CASE("invalidate HLSL shader cache - COMPILED_BINARY_CHANGED (reflection)") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {
#if defined(WIN32)
            if (dynamic_cast<DirectXRenderer*>(pGameWindow->getRenderer()) != nullptr) {
                const auto shaderPath = ProjectPaths::getPathToResDirectory(ResourceDirectory::ROOT) /
                                        "test" / "temp" / "shader_COMPILED_BINARY_CHANGED_test.hlsl";

                // Create temporary shader file.
                std::ofstream shaderFile(shaderPath);
                REQUIRE(shaderFile.is_open());
                shaderFile << "float4 ps(float4 vPos : SV_POSITION) : SV_Target\n"
                              "{\n"
                              "return float4(1.0f, 1.0f, 1.0f, 1.0f);\n"
                              "}\n";
                shaderFile.close();

                const auto sShaderUniqueName = std::string("test shader");

                ShaderDescription description{
                    sShaderUniqueName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "ps", {}};

                // Compile.
                auto compileResult = ShaderPack::compileShaderPack(pGameWindow->getRenderer(), description);
                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult)) {
                    std::string sErrorMessage;
                    if (std::holds_alternative<std::string>(compileResult)) {
                        sErrorMessage = std::get<std::string>(compileResult);
                    } else {
                        sErrorMessage = std::get<Error>(compileResult).getFullErrorMessage();
                    }
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(compileResult));
                }

                // Should find valid cache.
                std::optional<ShaderCacheInvalidationReason> cacheInvalidationReason;
                auto cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                if (!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult)) {
                    std::string sErrorMessage = std::get<Error>(cacheResult).getFullErrorMessage();
                    INFO(sErrorMessage);
                    REQUIRE(std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                }
                REQUIRE(!cacheInvalidationReason.has_value());
                const auto pShaderPack = std::get<std::shared_ptr<ShaderPack>>(cacheResult);

                // Get path to shader bytecode.
                const std::filesystem::path pathToShaderReflection =
                    (ProjectPaths::getPathToCompiledShadersDirectory() / sShaderUniqueName /
                     ShaderFilesystemPaths::getShaderCacheBaseFileName())
                        .string() +
                    ".reflection";
                REQUIRE(std::filesystem::exists(pathToShaderReflection));

                // Now manually change shader reflection.
                // It's enough to modify reflection of just one shader configuration for cache to be invalid.
                std::ofstream shaderReflectionFile(pathToShaderReflection);
                REQUIRE(shaderReflectionFile.is_open());
                shaderReflectionFile << "Hello World!";
                shaderReflectionFile.close();

                // Cache should be invalidated.
                cacheResult = ShaderPack::createFromCache(
                    pGameWindow->getRenderer(), description, cacheInvalidationReason);

                REQUIRE(!std::holds_alternative<std::shared_ptr<ShaderPack>>(cacheResult));
                REQUIRE(cacheInvalidationReason.has_value());
                REQUIRE(
                    cacheInvalidationReason.value() ==
                    ShaderCacheInvalidationReason::COMPILED_BINARY_CHANGED);

                // Cleanup.
                std::filesystem::remove(shaderPath);
            }
#endif
            pGameWindow->close();
        }
        virtual ~TestGameInstance() override {}
    };

    auto result = Window::getBuilder().withVisibility(false).build();
    if (std::holds_alternative<Error>(result)) {
        Error error = std::get<Error>(std::move(result));
        error.addCurrentLocationToErrorStack();
        INFO(error.getFullErrorMessage());
        REQUIRE(false);
    }

    const std::unique_ptr<Window> pMainWindow = std::get<std::unique_ptr<Window>>(std::move(result));
    pMainWindow->processEvents<TestGameInstance>();
}
