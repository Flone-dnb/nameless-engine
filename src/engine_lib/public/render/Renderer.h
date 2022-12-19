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

namespace ne {
    class Game;
    class Window;
    class PsoManager;

    /**
     * Describes texture filtering mode.
     */
    enum class TextureFilteringMode : int { POINT = 0, LINEAR = 1, ANISOTROPIC = 2 };

    /**
     * Contains width, height and refresh rate.
     */
    struct RenderMode {
        /** Width in pixels. */
        int iWidth;
        /** Height in pixels. */
        int iHeight;
        /** Refresh rate numerator. */
        int iRefreshRateNumerator;
        /** Refresh rate denominator. */
        int iRefreshRateDenominator;
    };

    /**
     * Describes anti-aliasing settings.
     */
    struct Antialiasing {
        /** Whether the AA is enabled or not. */
        bool bIsEnabled;
        /**
         * Sample count of AA, for MSAA valid values are:
         * 2 (x2 sample count - good quality) or 4 (x4 sample count - best quality).
         */
        int iSampleCount;
    };

    /**
     * Defines a base class for renderers to implement.
     */
    class Renderer {
    public:
        /**
         * Constructor.
         *
         * @param pGame   Game object that owns this renderer.
         */
        Renderer(Game* pGame);

        Renderer() = delete;
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        virtual ~Renderer() = default;

        /**
         * Sets texture filtering.
         *
         * @param settings Texture filtering mode.
         *
         * @return Error if something went wrong.
         */
        virtual std::optional<Error> setTextureFiltering(const TextureFilteringMode& settings) = 0;

        /**
         * Sets antialiasing settings.
         *
         * @param settings Antialiasing settings.
         *
         * @return Error if something went wrong.
         */
        virtual std::optional<Error> setAntialiasing(const Antialiasing& settings) = 0;

        /**
         * Sets backbuffer color.
         *
         * @param fillColor Color that the backbuffer will be filled with before rendering a frame.
         * 4 values correspond to RGBA parameters.
         *
         * @return Error if something went wrong.
         */
        virtual std::optional<Error> setBackbufferFillColor(float fillColor[4]) = 0;

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @return Error if can't find any GPU that supports this renderer,
         * vector with GPU names if successful.
         */
        virtual std::variant<std::vector<std::string>, Error> getSupportedGpus() const = 0;

        /**
         * Returns a list of supported render resolution.
         *
         * @return Error if something went wrong, otherwise render mode.
         */
        virtual std::variant<std::vector<RenderMode>, Error> getSupportedRenderResolutions() const = 0;

        /**
         * Returns backbuffer resolution.
         *
         * @return Width and height.
         */
        virtual std::pair<int, int> getRenderResolution() const = 0;

        /**
         * Returns the name of the GPU that is being currently used.
         *
         * @return Name of the GPU.
         */
        virtual std::string getCurrentlyUsedGpuName() const = 0;

        /**
         * Returns currently used antialiasing settings.
         *
         * @return Antialiasing settings.
         */
        virtual Antialiasing getAntialiasing() const = 0;

        /**
         * Returns currently used texture filtering mode.
         *
         * @return Texture filtering mode.
         */
        virtual TextureFilteringMode getTextureFiltering() const = 0;

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
         */
        virtual void flushCommandQueue() = 0;

        /** Draw new frame. */
        virtual void drawNextFrame() = 0;

        /**
         * Returns the current vertex shader configuration (shader settings,
         * represented by a bunch of predefined macros).
         *
         * @return Vertex shader configuration.
         */
        std::set<ShaderParameter> getVertexShaderConfiguration() const;

        /**
         * Returns the current pixel shader configuration (shader settings,
         * represented by a bunch of predefined macros).
         *
         * @return Pixel shader configuration.
         */
        std::set<ShaderParameter> getPixelShaderConfiguration() const;

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

    protected:
        /** Update internal resources for the next frame. */
        virtual void updateResourcesForNextFrame() = 0;

        /**
         * Writes current renderer configuration to disk.
         */
        virtual void writeConfigurationToConfigFile() = 0;

        /**
         * Reads renderer configuration from disk.
         * If at least one key is empty or incorrect the renderer should
         * throw an error.
         */
        virtual void readConfigurationFromConfigFile() = 0;

        /**
         * Returns the amount of buffers the swap chain has.
         *
         * @return The amount of buffers the swap chain has.
         */
        static consteval unsigned int getSwapChainBufferCount();

        /**
         * Used to determine if the configuration file exists on the disk.
         *
         * @return 'true' if configuration file exists, 'false' otherwise.
         */
        static bool isConfigurationFileExists();

        /**
         * Returns path of the configuration file used by the renderer.
         *
         * @return Configuration file path, file might not exist, but the directories will be created.
         */
        static std::filesystem::path getRendererConfigurationFilePath();

        /**
         * Sets the current vertex shader configuration (settings).
         *
         * @param vertexShaderConfiguration Vertex shader configuration.
         */
        void setVertexShaderConfiguration(const std::set<ShaderParameter>& vertexShaderConfiguration);

        /**
         * Sets the current pixel shader configuration (settings).
         *
         * @param pixelShaderConfiguration Pixel shader configuration.
         */
        void setPixelShaderConfiguration(const std::set<ShaderParameter>& pixelShaderConfiguration);

        /**
         * Returns name of the section used in configuration file.
         *
         * @return Section name.
         */
        static const char* getConfigurationSectionGpu();

        /**
         * Returns name of the section used in configuration file.
         *
         * @return Section name.
         */
        static const char* getConfigurationSectionResolution();

        /**
         * Returns name of the section used in configuration file.
         *
         * @return Section name.
         */
        static const char* getConfigurationSectionRefreshRate();

        /**
         * Returns name of the section used in configuration file.
         *
         * @return Section name.
         */
        static const char* getConfigurationSectionAntialiasing();

        /**
         * Returns name of the section used in configuration file.
         *
         * @return Section name.
         */
        static const char* getConfigurationSectionVSync();

        /**
         * Returns name of the section used in configuration file.
         *
         * @return Section name.
         */
        static const char* getConfigurationSectionTextureFiltering();

        /**
         * Returns name of the category the renderer uses for logging.
         *
         * @return Category name.
         */
        static const char* getRendererLoggingCategory();

    private:
        /** Lock when reading or writing to render resources. Usually used with @ref flushCommandQueue. */
        std::recursive_mutex mtxRwRenderResources;

        /** Used to compile shaders. */
        std::unique_ptr<ShaderManager> pShaderManager;

        /** Used to store various graphics and compute PSOs. */
        std::unique_ptr<PsoManager> pPsoManager;

        /** Vertex shader parameters. */
        std::set<ShaderParameter> currentVertexShaderConfiguration;

        /** Pixel shader parameters. */
        std::set<ShaderParameter> currentPixelShaderConfiguration;

        /** Do not delete (free) this pointer. Game object that owns this renderer. */
        Game* pGame = nullptr;

        /** Number of buffers in swap chain. */
        static constexpr unsigned int iSwapChainBufferCount = 2;

        /** File name used to store renderer configuration. */
        inline static const char* sRendererConfigurationFileName = "render";

        /** Name of the section (used in configuration) for GPU settings. */
        inline static const char* sConfigurationSectionGpu = "GPU";

        /** Name of the section (used in configuration) for resolution settings. */
        inline static const char* sConfigurationSectionResolution = "resolution";

        /** Name of the section (used in configuration) for refresh rate settings. */
        inline static const char* sConfigurationSectionRefreshRate = "refresh_rate";

        /** Name of the section (used in configuration) for anti-aliasing settings. */
        inline static const char* sConfigurationSectionAntialiasing = "anti_aliasing";

        /** Name of the section (used in configuration) for vsync settings. */
        inline static const char* sConfigurationSectionVSync = "vsync";

        /** Name of the section (used in configuration) for texture filtering settings. */
        inline static const char* sConfigurationSectionTextureFiltering = "texture_filtering";

        /** Name of the category used for logging. */
        inline static const char* sRendererLogCategory = "Renderer";
    };

    consteval unsigned Renderer::getSwapChainBufferCount() { return iSwapChainBufferCount; }
} // namespace ne
