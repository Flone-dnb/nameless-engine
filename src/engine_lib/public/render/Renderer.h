#pragma once

// Standard.
#include <variant>
#include <vector>
#include <string>
#include <filesystem>
#include <memory>

// Custom.
#include "misc/Error.h"
#include "materials/ShaderManager.h"
#include "render/RenderSettings.h"
#include "misc/ProjectPaths.h"

namespace ne {
    class Game;
    class Window;
    class PsoManager;
    class ShaderConfiguration;
    class RenderSettings;

    /**
     * Defines a base class for renderers to implement.
     */
    class Renderer {
    public:
        Renderer() = delete;
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        virtual ~Renderer() = default;

        /**
         * Creates a new platform-specific renderer.
         *
         * @param pGame Game object that will own this renderer.
         *
         * @return Created renderer.
         */
        static std::unique_ptr<Renderer> create(Game* pGame);

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @return Error if can't find any GPU that supports this renderer,
         * vector with GPU names if successful.
         */
        virtual std::variant<std::vector<std::string>, Error> getSupportedGpuNames() const = 0;

        /**
         * Returns a list of supported render resolution (pairs of width and height).
         *
         * @return Error if something went wrong, otherwise render mode.
         */
        virtual std::variant<std::vector<std::pair<unsigned int, unsigned int>>, Error>
        getSupportedRenderResolutions() const = 0;

        /**
         * Returns render settings that can be configured.
         *
         * @return Do not delete (free) returned pointer. Non-owning pointer to render settings.
         */
        std::pair<std::recursive_mutex, std::unique_ptr<RenderSettings>>* getRenderSettings();

        /**
         * Returns the name of the GPU that is being currently used.
         *
         * @return Name of the GPU.
         */
        virtual std::string getCurrentlyUsedGpuName() const = 0;

        /**
         * Returns total video memory size (VRAM) in megabytes.
         *
         * @return Total video memory size in megabytes.
         */
        virtual size_t getTotalVideoMemoryInMb() const = 0;

        /**
         * Returns used video memory size (VRAM) in megabytes.
         *
         * @return Used video memory size in megabytes.
         */
        virtual size_t getUsedVideoMemoryInMb() const = 0;

        /**
         * Blocks the current thread until the GPU finishes executing all queued commands up to this point.
         *
         * @remark Typically used with @ref getRenderResourcesMutex.
         */
        virtual void flushCommandQueue() = 0;

        /**
         * Returns the current shader configuration (shader settings,
         * represented by a bunch of predefined macros).
         *
         * Must be used with mutex.
         *
         * @return Do not delete (free) returned pointer. Shader configuration.
         */
        std::pair<std::recursive_mutex, std::unique_ptr<ShaderConfiguration>>* getShaderConfiguration();

        /**
         * Returns the window that we render to.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Window we render to.
         */
        Window* getWindow() const;

        /**
         * Game object that owns this renderer.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Game object.
         */
        Game* getGame() const;

        /**
         * Returns shader manager used to compile shaders.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return Shader manager.
         */
        ShaderManager* getShaderManager() const;

        /**
         * Returns PSO manager used to store graphics and compute PSOs.
         *
         * @warning Do not delete (free) returned pointer.
         *
         * @return PSO manager.
         */
        PsoManager* getPsoManager() const;

        /**
         * Returns mutex used when reading or writing to render resources.
         * Usually used with @ref flushCommandQueue.
         *
         * @return Mutex.
         */
        std::recursive_mutex* getRenderResourcesMutex();

        /**
         * Returns name of the directory we use to store render-specific settings.
         *
         * @return Directory name.
         */
        static const char* getRenderConfigurationDirectoryName();

    protected:
        // Only window should be able to request new frames to be drawn.
        friend class Window;

        // Settings will change shader configuration and other render settings.
        friend class RenderSetting;

        // Can update shader configuration.
        friend class ShaderConfiguration;

        /**
         * Constructor.
         *
         * @param pGame Game object that owns this renderer.
         */
        Renderer(Game* pGame);

        /**
         * Returns the amount of buffers the swap chain has.
         *
         * @return The amount of buffers the swap chain has.
         */
        static consteval unsigned int getSwapChainBufferCount() { return iSwapChainBufferCount; }

        /** Update internal resources for the next frame. */
        virtual void updateResourcesForNextFrame() = 0;

        /** Draw new frame. */
        virtual void drawNextFrame() = 0;

        /**
         * Recreates all render buffers to match current settings.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> updateRenderBuffers() = 0;

        /**
         * (Re)creates depth/stencil buffer.
         *
         * @remark Make sure that the old depth/stencil buffer (if was) is not used by the GPU.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] virtual std::optional<Error> createDepthStencilBuffer() = 0;

        /**
         * Tells whether the renderer is initialized or not
         * (whether it's safe to use renderer functionality or not).
         *
         * @return Whether the renderer is initialized or not.
         */
        virtual bool isInitialized() const = 0;

        /** Initializes render settings, must be called in the beginning of the constructor. */
        void initializeRenderSettings();

    private:
        /**
         * Updates the current shader configuration (settings) based on the current value
         * from @ref getShaderConfiguration.
         *
         * @remark Flushes the command queue and recreates PSOs' internal resources so that they
         * will use new shader configuration.
         */
        void updateShaderConfiguration();

        /** Lock when reading or writing to render resources. Usually used with @ref flushCommandQueue. */
        std::recursive_mutex mtxRwRenderResources;

        /** Used to compile shaders. */
        std::unique_ptr<ShaderManager> pShaderManager;

        /** Used to store various graphics and compute PSOs. */
        std::unique_ptr<PsoManager> pPsoManager;

        /**
         * Shader parameters.
         * Must be used with mutex.
         */
        std::pair<std::recursive_mutex, std::unique_ptr<ShaderConfiguration>> mtxShaderConfiguration;

        /**
         * Render setting objects that configure the renderer.
         * Must be used with mutex.
         */
        std::pair<std::recursive_mutex, std::unique_ptr<RenderSettings>> mtxRenderSettings;

        /** Do not delete (free) this pointer. Game object that owns this renderer. */
        Game* pGame = nullptr;

        /** Number of buffers in swap chain. */
        static constexpr unsigned int iSwapChainBufferCount = 2;

        /** Directory name used to store renderer configuration. */
        inline static const char* sRendererConfigurationDirectoryName = "render";

        /** Name of the category used for logging. */
        inline static const char* sRendererLogCategory = "Renderer";
    };

    /** Small class that combines all render setting objects. */
    class RenderSettings {
    public:
        RenderSettings() = delete;

        RenderSettings(const RenderSetting&) = delete;
        RenderSettings& operator=(const RenderSetting&) = delete;

        /**
         * Returns anti-aliasing settings.
         *
         * @return Do not delete (free) returned pointer. Anti-aliasing settings.
         */
        AntialiasingRenderSetting* getAntialiasingSettings() { return pAntialiasingSettings.get(); }

        /**
         * Returns texture filtering settings.
         *
         * @return Do not delete (free) returned pointer. Texture filtering settings.
         */
        TextureFilteringRenderSetting* getTextureFilteringSettings() {
            return pTextureFilteringSettings.get();
        }

        /**
         * Returns screen settings.
         *
         * @return Do not delete (free) returned pointer. Screen settings.
         */
        ScreenRenderSetting* getScreenSettings() { return pScreenSettings.get(); }

    private:
        // Only renderer can initialize render settings.
        friend class Renderer;

        /**
         * Initializes render settings.
         *
         * @param pRenderer Game's renderer.
         */
        RenderSettings(Renderer* pRenderer) { this->pRenderer = pRenderer; }

        /** Initializes render settings. */
        void initialize() {
            if (bIsInitialized)
                return;

            bIsInitialized = true;

            pAntialiasingSettings = setupRenderSetting<AntialiasingRenderSetting>(pRenderer);
            pTextureFilteringSettings = setupRenderSetting<TextureFilteringRenderSetting>(pRenderer);
            pScreenSettings = setupRenderSetting<ScreenRenderSetting>(pRenderer);
        }

        /**
         * Looks if the configuration file for the specified setting exists and deserializes it,
         * otherwise creates a new object and returns it.
         *
         * @param pRenderer Renderer to use.
         *
         * @return Render setting object.
         */
        template <typename T>
            requires std::derived_from<T, RenderSetting> && std::derived_from<T, Serializable>
        std::shared_ptr<T> setupRenderSetting(Renderer* pRenderer) {
            // Construct path to the directory with config files.
            const auto pathToConfigDirectory = ProjectPaths::getDirectoryForEngineConfigurationFiles() /
                                               pRenderer->getRenderConfigurationDirectoryName();

            // Construct path to config file.
            const auto pathToConfigFile = pathToConfigDirectory / T::getConfigurationFileName(true);

            std::shared_ptr<T> pRenderSetting;
            bool bDeserializedWithoutIssues = false;

            // See if config file exists.
            if (std::filesystem::exists(pathToConfigFile)) {
                // Try to deserialize.
                auto result = Serializable::deserialize<std::shared_ptr, T>(pathToConfigFile);
                if (std::holds_alternative<Error>(result)) {
                    auto error = std::get<Error>(std::move(result));
                    error.addEntry();
                    Logger::get().error(
                        fmt::format(
                            "failed to deserialize render settings from the file \"{}\", using default "
                            "settings instead, error: \"{}\"",
                            pathToConfigFile.string(),
                            error.getError()),
                        sRenderSettingsLogCategory);

                    // Just create a new object with default settings.
                    pRenderSetting = std::make_shared<T>();
                } else {
                    // Use the deserialized object.
                    pRenderSetting = std::get<std::shared_ptr<T>>(std::move(result));
                    bDeserializedWithoutIssues = true;
                }
            } else {
                // Just create a new object with default settings.
                pRenderSetting = std::make_shared<T>();
            }

            // Initialize the setting.
            const auto pParentSettings = dynamic_cast<RenderSetting*>(pRenderSetting.get());
            pParentSettings->setRenderer(pRenderer);

            // Save if no config existed.
            if (!bDeserializedWithoutIssues) {
                auto optionalError = pParentSettings->saveConfigurationToDisk();
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addEntry();
                    Logger::get().error(
                        fmt::format("failed to save new render settings, error: \"{}\"", error.getError()),
                        sRenderSettingsLogCategory);
                }
            }

            // Apply the configuration.
            pParentSettings->updateRendererConfiguration();

            return pRenderSetting;
        }

        /** Anti-aliasing settings. */
        std::shared_ptr<AntialiasingRenderSetting> pAntialiasingSettings;

        /** Texture filtering settings. */
        std::shared_ptr<TextureFilteringRenderSetting> pTextureFilteringSettings;

        /** Screen related settings. */
        std::shared_ptr<ScreenRenderSetting> pScreenSettings;

        /** Used renderer. */
        Renderer* pRenderer = nullptr;

        /** Whether the @ref initialize was called or not. */
        bool bIsInitialized = false;

        /** Name of the category used for logging. */
        inline static const char* sRenderSettingsLogCategory = "Render Settings";
    };

    /** Describes shader parameters. */
    class ShaderConfiguration {
    public:
        ShaderConfiguration() = delete;

        /**
         * Saves render to use.
         *
         * @param pRenderer Renderer to use.
         */
        ShaderConfiguration(Renderer* pRenderer) { this->pRenderer = pRenderer; }

        /**
         * Updates the current shader configuration (settings) for the current renderer based on the current
         * values from this struct.
         *
         * @remark Flushes the command queue and recreates PSOs' internal resources so that they
         * will use new shader configuration.
         */
        void updateShaderConfiguration() { pRenderer->updateShaderConfiguration(); }

        /** Vertex shader parameters. */
        std::set<ShaderParameter> currentVertexShaderConfiguration;

        /** Pixel shader parameters. */
        std::set<ShaderParameter> currentPixelShaderConfiguration;

    private:
        /** Used renderer. */
        Renderer* pRenderer = nullptr;
    };
} // namespace ne
