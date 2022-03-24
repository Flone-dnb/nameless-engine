#pragma once

// STL.
#include <variant>
#include <vector>
#include <string>

// Custom.
#include "misc/Error.h"

namespace ne {
    /**
     * Contains width, height and refresh rate.
     */
    struct RenderMode {
        int iWidth;
        int iHeight;
        int iRefreshRateNumerator;
        int iRefreshRateDenominator;
    };
    /**
     * Defines an interface for renderers to implement.
     */
    class IRenderer {
    public:
        IRenderer() = default;

        IRenderer(const IRenderer &) = delete;
        IRenderer &operator=(const IRenderer &) = delete;

        virtual ~IRenderer() = default;

        virtual void update() = 0;
        virtual void drawFrame() = 0;

        /**
         * Looks for video adapters (GPUs) that support this renderer.
         *
         * @return Error if can't find any GPU that supports this renderer,
         * vector with GPU names if successful.
         */
        virtual std::variant<std::vector<std::wstring>, Error> getSupportedGpus() const = 0;

        /**
         * Returns a list of supported render resolution.
         *
         * @return Error if something went wrong, otherwise render mode.
         */
        virtual std::variant<std::vector<RenderMode>, Error> getSupportedRenderResolutions() const = 0;

        /**
         * Returns backbuffer resolution.
         *
         * @return A pair of X and Y pixels.
         */
        virtual std::pair<int, int> getRenderResolution() const = 0;

        /**
         * Returns the name of the GPU that is being currently used.
         *
         * @return Name of the GPU.
         */
        virtual std::wstring getCurrentlyUsedGpuName() const = 0;

    protected:
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
         * Used to determine if the configuration file exists on the disk.
         */
        static bool isConfigurationFileExists();

        /**
         * Returns name of the configuration file used by the renderer.
         *
         * @return Configuration file name.
         */
        static const char *getRendererConfigurationFileName();

        /**
         * Returns name of the section used in configuration file.
         *
         * @return Section name.
         */
        static const char *getConfigurationSectionGpu();

        /**
         * Returns name of the section used in configuration file.
         *
         * @return Section name.
         */
        static const char *getConfigurationSectionResolution();

        /**
         * Returns name of the section used in configuration file.
         *
         * @return Section name.
         */
        static const char *getConfigurationSectionRefreshRate();

        /**
         * Returns name of the section used in configuration file.
         *
         * @return Section name.
         */
        static const char *getConfigurationSectionAntialiasing();

    private:
        inline static const char *sRendererConfigurationFileName = "render";
        inline static const char *sConfigurationSectionGpu = "GPU";
        inline static const char *sConfigurationSectionResolution = "resolution";
        inline static const char *sConfigurationSectionRefreshRate = "refresh rate";
        inline static const char *sConfigurationSectionAntialiasing = "anti-aliasing";
    };
} // namespace ne
