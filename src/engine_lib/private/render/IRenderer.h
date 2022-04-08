#pragma once

// STL.
#include <variant>
#include <vector>
#include <string>
#include <filesystem>

// Custom.
#include "misc/Error.h"

namespace ne {
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
         * Quality of AA, for MSAA valid values are:
         * 2 (x2 sample count - good quality) or 4 (x4 sample count - best quality).
         */
        int iQuality;
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
         * @return Width and height.
         */
        virtual std::pair<int, int> getRenderResolution() const = 0;

        /**
         * Returns the name of the GPU that is being currently used.
         *
         * @return Name of the GPU.
         */
        virtual std::wstring getCurrentlyUsedGpuName() const = 0;

        /**
         * Returns currently used AA settings.
         *
         * @return AA settings.
         */
        virtual Antialiasing getCurrentAntialiasing() const = 0;

    protected:
        /** Update internal resources for next frame. */
        virtual void update() = 0;

        /** Draw new frame. */
        virtual void drawFrame() = 0;

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
        /** File name used to store renderer configuration. */
        inline static const char *sRendererConfigurationFileName = "render";

        /** Name of the section (used in configuration) for GPU settings. */
        inline static const char *sConfigurationSectionGpu = "GPU";

        /** Name of the section (used in configuration) for resolution settings. */
        inline static const char *sConfigurationSectionResolution = "resolution";

        /** Name of the section (used in configuration) for refresh rate settings. */
        inline static const char *sConfigurationSectionRefreshRate = "refresh rate";

        /** Name of the section (used in configuration) for anti-aliasing settings. */
        inline static const char *sConfigurationSectionAntialiasing = "anti-aliasing";
    };
} // namespace ne
