// Custom.
#include "game/GameInstance.h"
#include "game/Window.h"
#include "render/vulkan/VulkanRenderer.h"
#include "shader/glsl/DescriptorSetLayoutGenerator.h"
#include "shader/glsl/GlslShader.h"

// External.
#include "catch2/catch_test_macros.hpp"

const inline std::filesystem::path shaderPathDir =
    ne::ProjectPaths::getPathToResDirectory(ne::ResourceDirectory::ROOT) / "test" / "temp";
const inline std::filesystem::path shaderPath = shaderPathDir / "test_shader.glsl";
const inline auto pTestVertexShaderName = "test vertex shader";
const inline auto pTestPixelShaderName = "test pixel shader";

TEST_CASE("2 resources with the same name but different bindings cause error") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}

        virtual void onGameStarted() override {
            // Make sure we are using Vulkan renderer.
            if (dynamic_cast<VulkanRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-Vulkan renderer.
                getWindow()->close();
                SKIP();
            }

            // Create world.
            createWorld([this](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) [[unlikely]] {
                    auto error = optionalWorldError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create directory for temp shaders.
                if (!std::filesystem::exists(shaderPathDir)) {
                    std::filesystem::create_directory(shaderPathDir);
                }

                // Prepare vertex shader.
                std::ofstream vertexShaderFile(shaderPath);
                REQUIRE(vertexShaderFile.is_open());
                vertexShaderFile << "#version 450\n"
                                    "layout(binding = 0) uniform SomeData {\n"
                                    "float someFloat;"
                                    "} someData;"
                                    "void main()\n"
                                    "{\n"
                                    "gl_Position = vec4(someData.someFloat, 0.0F, 0.0F, 0.0F);\n"
                                    "}\n";
                vertexShaderFile.close();
                ShaderDescription vertexDescription{
                    pTestVertexShaderName, shaderPath, ShaderType::VERTEX_SHADER, {}, "main", {}};

                // Compile vertex shader.
                auto compileResult =
                    ShaderPack::compileShaderPack(getWindow()->getRenderer(), vertexDescription);
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
                const auto pVertexShaderPack =
                    std::get<std::shared_ptr<ShaderPack>>(std::move(compileResult));

                // Prepare fragment shader.
                std::ofstream fragmentShaderFile(shaderPath);
                REQUIRE(fragmentShaderFile.is_open());
                fragmentShaderFile << "#version 450\n"
                                      "layout(binding = 1) uniform SomeData {\n" // different binding index
                                      "float someFloat;"
                                      "} someData;"
                                      "layout(location = 0) out vec4 outColor;\n"
                                      "void main()\n"
                                      "{\n"
                                      "outColor = vec4(someData.someFloat, 0.0F, 0.0F, 0.0F);\n"
                                      "}\n";
                fragmentShaderFile.close();
                ShaderDescription fragmentDescription{
                    pTestVertexShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "main", {}};

                // Compile fragment shader.
                compileResult =
                    ShaderPack::compileShaderPack(getWindow()->getRenderer(), fragmentDescription);
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
                const auto pFragmentShaderPack =
                    std::get<std::shared_ptr<ShaderPack>>(std::move(compileResult));

                // Set dummy configuration.
                pVertexShaderPack->getInternalResources()->second.bIsRenderConfigurationSet = true;
                pFragmentShaderPack->getInternalResources()->second.bIsRenderConfigurationSet = true;

                // Get vertex shader.
                std::set<ShaderMacro> fullConfig;
                const auto pVertexShader =
                    std::dynamic_pointer_cast<GlslShader>(pVertexShaderPack->getShader({}, fullConfig));
                REQUIRE(pVertexShader != nullptr);

                // Get fragment shader.
                const auto pFragmentShader =
                    std::dynamic_pointer_cast<GlslShader>(pVertexShaderPack->getShader({}, fullConfig));
                REQUIRE(pFragmentShader != nullptr);

                // Make sure vertex shader bytecode is generated without errors.
                auto shaderBytecode = pVertexShader->getCompiledBytecode();
                if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
                    auto err = std::get<Error>(std::move(shaderBytecode));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Make sure fragment shader bytecode is generated without errors.
                shaderBytecode = pFragmentShader->getCompiledBytecode();
                if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
                    auto err = std::get<Error>(std::move(shaderBytecode));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Generate one descriptor layout from both shaders.
                auto layoutResult = DescriptorSetLayoutGenerator::generateGraphics(
                    dynamic_cast<VulkanRenderer*>(getWindow()->getRenderer()),
                    pVertexShader.get(),
                    pFragmentShader.get());

                // Should fail since we have 2 resources with the same name but on different bindings
                // the engine will not be able to differentiate them since we only use names.
                REQUIRE(std::holds_alternative<Error>(layoutResult));

                // Remove temp shader.
                if (std::filesystem::exists(shaderPath)) {
                    std::filesystem::remove(shaderPath);
                }

                // Release bytecode.
                pVertexShader->releaseShaderDataFromMemoryIfLoaded();
                pFragmentShader->releaseShaderDataFromMemoryIfLoaded();

                getWindow()->close();
            });
        }

        virtual ~TestGameInstance() override {}

    private:
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

TEST_CASE("2 resources with the same name/bindings but different types cause error") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}

        virtual void onGameStarted() override {
            // Make sure we are using Vulkan renderer.
            if (dynamic_cast<VulkanRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-Vulkan renderer.
                getWindow()->close();
                SKIP();
            }

            // Create world.
            createWorld([this](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) [[unlikely]] {
                    auto error = optionalWorldError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create directory for temp shaders.
                if (!std::filesystem::exists(shaderPathDir)) {
                    std::filesystem::create_directory(shaderPathDir);
                }

                // Prepare vertex shader.
                std::ofstream vertexShaderFile(shaderPath);
                REQUIRE(vertexShaderFile.is_open());
                vertexShaderFile << "#version 450\n"
                                    "layout(binding = 0) uniform SomeData {\n"
                                    "float someFloat;"
                                    "} someData;"
                                    "void main()\n"
                                    "{\n"
                                    "gl_Position = vec4(someData.someFloat, 0.0F, 0.0F, 0.0F);\n"
                                    "}\n";
                vertexShaderFile.close();
                ShaderDescription vertexDescription{
                    pTestVertexShaderName, shaderPath, ShaderType::VERTEX_SHADER, {}, "main", {}};

                // Compile vertex shader.
                auto compileResult =
                    ShaderPack::compileShaderPack(getWindow()->getRenderer(), vertexDescription);
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
                const auto pVertexShaderPack =
                    std::get<std::shared_ptr<ShaderPack>>(std::move(compileResult));

                // Prepare fragment shader.
                std::ofstream fragmentShaderFile(shaderPath);
                REQUIRE(fragmentShaderFile.is_open());
                fragmentShaderFile << "#version 450\n"
                                      "layout(binding = 0) readonly buffer SomeData {\n" // different type
                                      "float someFloat;"
                                      "} someData;"
                                      "layout(location = 0) out vec4 outColor;\n"
                                      "void main()\n"
                                      "{\n"
                                      "outColor = vec4(someData.someFloat, 0.0F, 0.0F, 0.0F);\n"
                                      "}\n";
                fragmentShaderFile.close();
                ShaderDescription fragmentDescription{
                    pTestVertexShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "main", {}};

                // Compile fragment shader.
                compileResult =
                    ShaderPack::compileShaderPack(getWindow()->getRenderer(), fragmentDescription);
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
                const auto pFragmentShaderPack =
                    std::get<std::shared_ptr<ShaderPack>>(std::move(compileResult));

                // Set dummy configuration.
                pVertexShaderPack->getInternalResources()->second.bIsRenderConfigurationSet = true;
                pFragmentShaderPack->getInternalResources()->second.bIsRenderConfigurationSet = true;

                // Get vertex shader.
                std::set<ShaderMacro> fullConfig;
                const auto pVertexShader =
                    std::dynamic_pointer_cast<GlslShader>(pVertexShaderPack->getShader({}, fullConfig));
                REQUIRE(pVertexShader != nullptr);

                // Get fragment shader.
                const auto pFragmentShader =
                    std::dynamic_pointer_cast<GlslShader>(pVertexShaderPack->getShader({}, fullConfig));
                REQUIRE(pFragmentShader != nullptr);

                // Make sure vertex shader bytecode is generated without errors.
                auto shaderBytecode = pVertexShader->getCompiledBytecode();
                if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
                    auto err = std::get<Error>(std::move(shaderBytecode));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Make sure fragment shader bytecode is generated without errors.
                shaderBytecode = pFragmentShader->getCompiledBytecode();
                if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
                    auto err = std::get<Error>(std::move(shaderBytecode));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Generate one descriptor layout from both shaders.
                auto layoutResult = DescriptorSetLayoutGenerator::generateGraphics(
                    dynamic_cast<VulkanRenderer*>(getWindow()->getRenderer()),
                    pVertexShader.get(),
                    pFragmentShader.get());

                // Should fail since we have 2 resources with the same name/bindings but different types
                // the engine will not be able to differentiate them since we only use names.
                REQUIRE(std::holds_alternative<Error>(layoutResult));

                // Remove temp shader.
                if (std::filesystem::exists(shaderPath)) {
                    std::filesystem::remove(shaderPath);
                }

                // Release bytecode.
                pVertexShader->releaseShaderDataFromMemoryIfLoaded();
                pFragmentShader->releaseShaderDataFromMemoryIfLoaded();

                getWindow()->close();
            });
        }

        virtual ~TestGameInstance() override {}

    private:
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

TEST_CASE("2 resources with different names but same type/binding cause error") {
    using namespace ne;

    class TestGameInstance : public GameInstance {
    public:
        TestGameInstance(Window* pGameWindow, GameManager* pGame, InputManager* pInputManager)
            : GameInstance(pGameWindow, pGame, pInputManager) {}

        virtual void onGameStarted() override {
            // Make sure we are using Vulkan renderer.
            if (dynamic_cast<VulkanRenderer*>(getWindow()->getRenderer()) == nullptr) {
                // Don't run this test on non-Vulkan renderer.
                getWindow()->close();
                SKIP();
            }

            // Create world.
            createWorld([this](const std::optional<Error>& optionalWorldError) {
                if (optionalWorldError.has_value()) [[unlikely]] {
                    auto error = optionalWorldError.value();
                    error.addCurrentLocationToErrorStack();
                    INFO(error.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Create directory for temp shaders.
                if (!std::filesystem::exists(shaderPathDir)) {
                    std::filesystem::create_directory(shaderPathDir);
                }

                // Prepare vertex shader.
                std::ofstream vertexShaderFile(shaderPath);
                REQUIRE(vertexShaderFile.is_open());
                vertexShaderFile << "#version 450\n"
                                    "layout(binding = 0) uniform SomeData1 {\n"
                                    "float someFloat;"
                                    "} someData1;"
                                    "void main()\n"
                                    "{\n"
                                    "gl_Position = vec4(someData1.someFloat, 0.0F, 0.0F, 0.0F);\n"
                                    "}\n";
                vertexShaderFile.close();
                ShaderDescription vertexDescription{
                    pTestVertexShaderName, shaderPath, ShaderType::VERTEX_SHADER, {}, "main", {}};

                // Compile vertex shader.
                auto compileResult =
                    ShaderPack::compileShaderPack(getWindow()->getRenderer(), vertexDescription);
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
                const auto pVertexShaderPack =
                    std::get<std::shared_ptr<ShaderPack>>(std::move(compileResult));

                // Prepare fragment shader.
                std::ofstream fragmentShaderFile(shaderPath);
                REQUIRE(fragmentShaderFile.is_open());
                fragmentShaderFile << "#version 450\n"
                                      "layout(binding = 0) uniform SomeData2 {\n" // different name
                                      "float someFloat;"
                                      "} someData2;"
                                      "layout(location = 0) out vec4 outColor;\n"
                                      "void main()\n"
                                      "{\n"
                                      "outColor = vec4(someData2.someFloat, 0.0F, 0.0F, 0.0F);\n"
                                      "}\n";
                fragmentShaderFile.close();
                ShaderDescription fragmentDescription{
                    pTestVertexShaderName, shaderPath, ShaderType::FRAGMENT_SHADER, {}, "main", {}};

                // Compile fragment shader.
                compileResult =
                    ShaderPack::compileShaderPack(getWindow()->getRenderer(), fragmentDescription);
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
                const auto pFragmentShaderPack =
                    std::get<std::shared_ptr<ShaderPack>>(std::move(compileResult));

                // Set dummy configuration.
                pVertexShaderPack->getInternalResources()->second.bIsRenderConfigurationSet = true;
                pFragmentShaderPack->getInternalResources()->second.bIsRenderConfigurationSet = true;

                // Get vertex shader.
                std::set<ShaderMacro> fullConfig;
                const auto pVertexShader =
                    std::dynamic_pointer_cast<GlslShader>(pVertexShaderPack->getShader({}, fullConfig));
                REQUIRE(pVertexShader != nullptr);

                // Get fragment shader.
                const auto pFragmentShader =
                    std::dynamic_pointer_cast<GlslShader>(pVertexShaderPack->getShader({}, fullConfig));
                REQUIRE(pFragmentShader != nullptr);

                // Make sure vertex shader bytecode is generated without errors.
                auto shaderBytecode = pVertexShader->getCompiledBytecode();
                if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
                    auto err = std::get<Error>(std::move(shaderBytecode));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Make sure fragment shader bytecode is generated without errors.
                shaderBytecode = pFragmentShader->getCompiledBytecode();
                if (std::holds_alternative<Error>(shaderBytecode)) [[unlikely]] {
                    auto err = std::get<Error>(std::move(shaderBytecode));
                    err.addCurrentLocationToErrorStack();
                    INFO(err.getFullErrorMessage());
                    REQUIRE(false);
                }

                // Generate one descriptor layout from both shaders.
                auto layoutResult = DescriptorSetLayoutGenerator::generateGraphics(
                    dynamic_cast<VulkanRenderer*>(getWindow()->getRenderer()),
                    pVertexShader.get(),
                    pFragmentShader.get());

                // Should fail since we have 2 resources with the same name/bindings but different types
                // the engine will not be able to differentiate them since we only use names.
                REQUIRE(std::holds_alternative<Error>(layoutResult));

                // Remove temp shader.
                if (std::filesystem::exists(shaderPath)) {
                    std::filesystem::remove(shaderPath);
                }

                // Release bytecode.
                pVertexShader->releaseShaderDataFromMemoryIfLoaded();
                pFragmentShader->releaseShaderDataFromMemoryIfLoaded();

                getWindow()->close();
            });
        }

        virtual ~TestGameInstance() override {}

    private:
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
