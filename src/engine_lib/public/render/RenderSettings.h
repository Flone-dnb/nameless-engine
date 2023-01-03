#pragma once

// Custom.
#include "io/Serializable.h"
#include "materials/ShaderParameter.h"
#include "materials/ShaderDescription.h"

#include "RenderSettings.generated.h"

namespace ne RNAMESPACE() {
    class Renderer;

    /**
     * Base class for render settings.
     *
     * @remark In order to create an object of this class you need to make sure that both game and renderer
     * objects exist.
     */
    class RCLASS(Guid("eb477c6d-cdc4-4b7a-9349-296fb38e6bfc")) RenderSetting : public Serializable {
    public:
        RenderSetting() = default;
        virtual ~RenderSetting() override = default;

        /**
         * Returns path to the file that is used to store setting configuration.
         *
         * @return Path to file.
         */
        virtual std::filesystem::path getPathToConfigurationFile() {
            throw std::runtime_error("not implemented");
        };

    protected:
        // "Manager" might request the configuration to be updated.
        friend class RenderSettings;

        /**
         * Sets renderer to be used.
         *
         * @remark It is essential to call this function to initialize the object.
         *
         * @param pRenderer Game's renderer.
         */
        void setRenderer(Renderer* pRenderer);

        /** Changes renderer settings/state to match the current settings. */
        virtual void updateRendererConfiguration() { throw std::runtime_error("not implemented"); };

        /**
         * Returns name of the category used for logging.
         *
         * @return Name of the category used for logging.
         */
        static const char* getRenderSettingLogCategory();

        /**
         * Recreates all render buffers to match current settings.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> updateRenderBuffers();

        /**
         * Returns game's renderer.
         *
         * @return Do not delete (free) returned pointer. `nullptr` if the renderer is not initialized yet,
         * otherwise game's renderer.
         */
        Renderer* getRenderer() const;

        /**
         * Tells whether the renderer is initialized or not
         * (whether it's safe to use renderer functionality or not).
         *
         * @return Whether the renderer is initialized or not.
         */
        bool isRendererInitialized() const;

    private:
        /** Do not delete (free) this pointer. Game's renderer. */
        Renderer* pRenderer = nullptr;

        /** Name of the category used for logging. */
        static inline const char* sRenderSettingLogCategory = "Render Setting";

        ne_RenderSetting_GENERATED
    };

    /** Describes the quality (sample count) of MSAA. */
    enum class MsaaQuality : int { MEDIUM = 2, HIGH = 4 };

    /** Controls anti-aliasing settings. */
    class RCLASS(Guid("9a9f58a6-fcea-4906-bb23-9aac508e6ea0")) AntialiasingRenderSetting
        : public RenderSetting {
    public:
        AntialiasingRenderSetting() = default;
        virtual ~AntialiasingRenderSetting() override = default;

        /**
         * Returns name of the file that is used to store setting configuration.
         *
         * @param bIncludeFileExtension Whether to include file extension in the returned string or not.
         *
         * @return Name of the file.
         */
        static std::string getConfigurationFileName(bool bIncludeFileExtension);

        /**
         * Returns path to the file that is used to store setting configuration.
         *
         * @return Path to file.
         */
        virtual std::filesystem::path getPathToConfigurationFile() override;

        /**
         * Enables/disables anti-aliasing (AA).
         *
         * @param bEnable Whether to enable AA or not.
         */
        void setEnabled(bool bEnable);

        /**
         * Sets anti-aliasing (AA) quality.
         *
         * @param quality AA quality.
         */
        void setQuality(MsaaQuality quality);

        /**
         * Tells whether anti-aliasing (AA) is currently enabled or not.
         *
         * @return Whether AA is enabled or not.
         */
        bool isEnabled() const;

        /**
         * Returns current anti-aliasing (AA) quality.
         *
         * @return Current AA quality.
         */
        MsaaQuality getQuality() const;

    protected:
        /** Changes renderer settings/state to match the current settings. */
        virtual void updateRendererConfiguration() override;

        /**
         * Called after the object was successfully deserialized.
         * Used to execute post-deserialization logic.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onAfterDeserialized() override;

        /**
         * Saves the current configuration to disk.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> saveConfigurationToDisk();

    private:
        /** Whether AA is enabled or not. */
        RPROPERTY(Serialize)
        bool bIsEnabled = true;

        /** Sample count of AA. */
        RPROPERTY(Serialize)
        int iSampleCount = static_cast<int>(MsaaQuality::HIGH);

        /** Name of the file we use to store configuration. */
        static inline const char* sAntialiasingConfigFileName = "antialiasing";

        ne_AntialiasingRenderSetting_GENERATED
    };

    /** Describes texture filtering mode. */
    enum class TextureFilteringMode : int { POINT = 0, LINEAR = 1, ANISOTROPIC = 2 };

    /** Controls texture filtering settings. */
    class RCLASS(Guid("071d0acb-3da1-408c-bf44-41653192d568")) TextureFilteringRenderSetting
        : public RenderSetting {
    public:
        TextureFilteringRenderSetting() = default;
        virtual ~TextureFilteringRenderSetting() override = default;

        /**
         * Returns name of the file that is used to store setting configuration.
         *
         * @param bIncludeFileExtension Whether to include file extension in the returned string or not.
         *
         * @return Name of the file.
         */
        static std::string getConfigurationFileName(bool bIncludeFileExtension);

        /**
         * Returns path to the file that is used to store setting configuration.
         *
         * @return Path to file.
         */
        virtual std::filesystem::path getPathToConfigurationFile() override;

        /**
         * Sets texture filtering mode to be used.
         *
         * @param mode Mode to use.
         */
        void setMode(TextureFilteringMode mode);

        /**
         * Returns currently used texture filtering mode.
         *
         * @return Texture filtering mode.
         */
        TextureFilteringMode getMode() const;

    protected:
        /** Changes renderer settings/state to match the current settings. */
        virtual void updateRendererConfiguration() override;

        /**
         * Called after the object was successfully deserialized.
         * Used to execute post-deserialization logic.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onAfterDeserialized() override;

        /**
         * Saves the current configuration to disk.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> saveConfigurationToDisk();

    private:
        /** Texture filtering mode. */
        RPROPERTY(Serialize)
        int iTextureFilteringMode = static_cast<int>(TextureFilteringMode::ANISOTROPIC);

        /** Name of the file we use to store configuration. */
        static inline const char* sTextureFilteringConfigFileName = "texture_filtering";

        ne_TextureFilteringRenderSetting_GENERATED
    };

    /** Controls various screen related settings such as render resolution, refresh rate, vsync, etc. */
    class RCLASS(Guid("168f1c4e-3e40-481b-b969-c50e886c8710")) ScreenRenderSetting : public RenderSetting {
    public:
        virtual ~ScreenRenderSetting() override = default;

        /**
         * Returns name of the file that is used to store setting configuration.
         *
         * @param bIncludeFileExtension Whether to include file extension in the returned string or not.
         *
         * @return Name of the file.
         */
        static std::string getConfigurationFileName(bool bIncludeFileExtension);

        /**
         * Returns path to the file that is used to store setting configuration.
         *
         * @return Path to file.
         */
        virtual std::filesystem::path getPathToConfigurationFile() override;

        /**
         * Sets the width and the height of the rendered images.
         *
         * @param resolution Width and height in pixels.
         */
        void setRenderResolution(std::pair<unsigned int, unsigned int> resolution);

        /**
         * Sets whether to enable vertical synchronization or not.
         *
         * @param bEnableVsync Whether to enable vertical synchronization or not.
         */
        void setVsyncEnabled(bool bEnableVsync);

        /**
         * Sets screen's refresh rate to use.
         *
         * @param refreshRate Numerator and denominator of the refresh rate to use.
         */
        void setRefreshRate(std::pair<unsigned int, unsigned int> refreshRate);

        /**
         * Sets GPU to be used next time the engine starts.
         *
         * @remark In order for this setting to be applied the engine needs to be restarted.
         *
         * @param sGpuName Name of the GPU to use.
         */
        void setGpuToUse(const std::string& sGpuName);

        /**
         * Sets GPU to be used next time the engine starts.
         *
         * @remark In order for this setting to be applied the engine needs to be restarted.
         *
         * @param iGpuIndex Index of the GPU to use.
         */
        void setGpuToUse(unsigned int iGpuIndex);

        /**
         * Returns currently used render resolution (width and height).
         *
         * @return Width and height in pixels.
         */
        std::pair<unsigned int, unsigned int> getRenderResolution();

        /**
         * Tells if vertical synchronization is currently enabled or not.
         *
         * @return Whether vertical synchronization is currently enabled or not.
         */
        bool isVsyncEnabled() const;

        /**
         * Returns currently used screen's refresh rate.
         *
         * @return Numerator and denominator of the refresh rate.
         */
        std::pair<unsigned int, unsigned int> getRefreshRate() const;

        /**
         * Returns name of the GPU being currently used.
         *
         * @return Empty if the renderer is not initialized yet, otherwise name of the GPU
         * being currently used.
         */
        std::string getUsedGpuName() const;

        /**
         * Same as @ref getUsedGpuName but returns index of the used GPU.
         *
         * @return Index of the GPU being currently used.
         */
        unsigned int getUsedGpuIndex() const;

    protected:
        /**
         * Saves the current configuration to disk.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> saveConfigurationToDisk();

        /** Changes renderer settings/state to match the current settings. */
        virtual void updateRendererConfiguration() override;

    private:
        /** Width of the back buffer. */
        RPROPERTY(Serialize)
        unsigned int iRenderResolutionWidth = 0;

        /** Height of the back buffer. */
        RPROPERTY(Serialize)
        unsigned int iRenderResolutionHeight = 0;

        /** Numerator of screen's refresh rate. */
        RPROPERTY(Serialize)
        unsigned int iRefreshRateNumerator = 0;

        /** Denominator of screen's refresh rate. */
        RPROPERTY(Serialize)
        unsigned int iRefreshRateDenominator = 0;

        /** Index of the used GPU. */
        RPROPERTY(Serialize)
        unsigned int iUsedGpuIndex = 0;

        /** Whether VSync is enabled or not. */
        RPROPERTY(Serialize)
        bool bIsVsyncEnabled = false;

        /** Name of the file we use to store configuration. */
        static inline const char* sScreenConfigFileName = "screen";

        ne_ScreenRenderSetting_GENERATED
    };
} // namespace ne RNAMESPACE()

File_RenderSettings_GENERATED
