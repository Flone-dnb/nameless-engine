#pragma once

// Custom.
#include "io/Serializable.h"

#include "RenderSettings.generated.h"

namespace ne RNAMESPACE() {
    class Renderer;

    /** Describes "types" of derived Renderer classes. */
    enum class RendererType : unsigned int { DIRECTX = 0, VULKAN = 1 };

    /** Describes the quality (sample count) of MSAA. */
    enum class MsaaState : int {
        // ! add new entries to `RenderSettings::onAfterDeserialized` !
        DISABLED = 1,
        MEDIUM = 2,
        HIGH = 4,
        VERY_HIGH = 8,
        // ! add new entries to `RenderSettings::onAfterDeserialized` !
    };

    /** Describes texture filtering mode. */
    enum class TextureFilteringMode : int { POINT = 0, LINEAR = 1, ANISOTROPIC = 2 };

    /** Controls renderer settings. */
    class RCLASS(Guid("eb477c6d-cdc4-4b7a-9349-296fb38e6bfc")) RenderSettings : public Serializable {
        // Renderer will initialize this object.
        friend class Renderer;

    public:
        RenderSettings() = default;
        virtual ~RenderSettings() override = default;

        /**
         * Returns path to the file that is used to store setting configuration.
         *
         * @return Path to file.
         */
        static std::filesystem::path getPathToConfigurationFile();

        /**
         * Sets anti-aliasing (AA) state.
         *
         * @warning Use @ref getMaxSupportedAntialiasingQuality to query for maximum supported AA quality,
         * if the specified quality is not supported an error will be logged.
         *
         * @param state AA state.
         */
        void setAntialiasingState(MsaaState state);

        /**
         * Sets texture filtering mode to be used.
         *
         * @param mode Mode to use.
         */
        void setTextureFilteringMode(TextureFilteringMode mode);

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
         * @remark Use Renderer to get available GPU names.
         *
         * @remark In order for this setting to be applied the engine needs to be restarted.
         *
         * @param sGpuName Name of the GPU to use.
         */
        void setGpuToUse(const std::string& sGpuName);

        /**
         * Changes renderer's config file setting about preferred renderer.
         *
         * @remark In order for this setting to be applied the engine needs to be restarted.
         *
         * @remark Note that this setting will be ignored if you use Window::setPreferredRenderer.
         *
         * @param preferredRenderer Renderer to prefer (test the first one) next time the game is started.
         */
        void setPreferredRenderer(RendererType preferredRenderer);

        /**
         * Returns current anti-aliasing (AA) quality.
         *
         * @return Returns `DISABLED` if AA is not supported (see @ref getMaxSupportedAntialiasingQuality),
         * otherwise current AA quality.
         */
        MsaaState getAntialiasingState() const;

        /**
         * Returns the maximum anti-aliasing quality that can be used on the picked
         * GPU (see `Renderer::getCurrentlyUsedGpuName`).
         *
         * @remark Note that the maximum supported AA quality can differ depending on the used GPU/renderer.
         *
         * @return Error if something went wrong,
         * otherwise `DISABLED` if AA is not supported or the maximum supported AA quality.
         */
        std::variant<MsaaState, Error> getMaxSupportedAntialiasingQuality() const;

        /**
         * Returns currently used texture filtering mode.
         *
         * @return Texture filtering mode.
         */
        TextureFilteringMode getTextureFilteringMode() const;

        /**
         * Returns currently used render resolution (width and height).
         *
         * @return Width and height in pixels.
         */
        std::pair<unsigned int, unsigned int> getRenderResolution() const;

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
         * Returns name of the GPU to use.
         *
         * @remark This value is taken from the renderer's settings file (see @ref setGpuToUse)
         * which sometimes might not represent the actual GPU being used.
         * Instead you might be interested in `Renderer::getCurrentlyUsedGpuName`.
         *
         * @return Name of the GPU specified in the settings.
         */
        std::string getGpuToUse() const;

    protected:
        /**
         * Called after the object was successfully deserialized.
         * Used to execute post-deserialization logic.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onAfterDeserialized() override;

    private:
        /**
         * Returns name of the file that is used to store setting configuration.
         *
         * @param bIncludeFileExtension Whether to include file extension in the returned string or not.
         *
         * @return Name of the file.
         */
        static std::string getConfigurationFileName(bool bIncludeFileExtension);

        /**
         * Sets renderer to be used.
         *
         * @remark It is essential to call this function to initialize the object.
         *
         * @param pRenderer Game's renderer.
         */
        void setRenderer(Renderer* pRenderer);

        /**
         * Called by the renderer when it has finished initializing its essential entities so
         * that RenderSettings can query maximum supported settings and clamp the current values (if needed).
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> clampSettingsToMaxSupported();

        /**
         * Saves the current configuration to disk.
         *
         * @remark Does nothing if @ref bAllowSavingConfigurationToDisk is `false`.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> saveConfigurationToDisk();

        /**
         * Notifies the renderer to change its internal state to match the current settings.
         *
         * @remark Does nothing if the renderer is not initialized yet. The renderer
         * will take values from the settings upon initialization.
         */
        void notifyRendererAboutChangedSettings();

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

        /** Name of the GPU to use. */
        RPROPERTY(Serialize)
        std::string sGpuToUse;

        /**
         * Stored used type of the renderer (DirectX/Vulkan/etc.).
         *
         * @remark Can be changed from the config file to change preferred renderer.
         */
        RPROPERTY(Serialize)
        unsigned int iRendererType = 0;

        /** Sample count of AA. */
        RPROPERTY(Serialize)
        int iAntialiasingSampleCount = static_cast<int>(MsaaState::HIGH);

        /** Texture filtering mode. */
        RPROPERTY(Serialize)
        int iTextureFilteringMode = static_cast<int>(TextureFilteringMode::ANISOTROPIC);

        /** Whether VSync is enabled or not. */
        RPROPERTY(Serialize)
        bool bIsVsyncEnabled = false;

        /** Do not delete (free) this pointer. Game's renderer. */
        Renderer* pRenderer = nullptr;

        /**
         * Defines whether or not changes to render settings trigger saving on disk.
         *
         * @remark Disabled by default as render settings can be modified by a renderer during
         * its initialization (some settings getting clamped/fixed due to render/hardware capabilities)
         * and because a renderer can fail to initialize (for example if the hardware does not support it)
         * we don't want any of these modifications to be saved. Once a renderer was initialized
         * the base Renderer class will enable saving on disk and will trigger a manual resave to apply
         * possibly fixed/clamped settings.
         */
        bool bAllowSavingConfigurationToDisk = false;

        /** Name of the file we use to store render settings. */
        static inline const char* sRenderSettingsConfigurationFileName = "render";

        ne_RenderSettings_GENERATED
    };
} // namespace ne RNAMESPACE()

File_RenderSettings_GENERATED
