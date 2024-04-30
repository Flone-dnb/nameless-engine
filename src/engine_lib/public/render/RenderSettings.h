#pragma once

// Custom.
#include "io/Serializable.h"

#include "RenderSettings.generated.h"

namespace ne RNAMESPACE() {
    class Renderer;

    /** Describes "types" of derived Renderer classes. */
    enum class RendererType : unsigned int { DIRECTX = 0, VULKAN = 1 };

    /** Describes the quality (sample count) of MSAA. */
    enum class AntialiasingQuality : unsigned int {
        DISABLED = 1,
        MEDIUM = 2,
        HIGH = 4,
        // no x8 MSAA because it has an absurd performance penalty with very little visual changes
    };

    /** Describes shadow map resolution in pixels (the actual shadow map resolution might be different). */
    enum class ShadowQuality : unsigned int {
        LOW = 256,
        MEDIUM = 512,
        HIGH = 1024,
    };

    /** Describes texture filtering mode. */
    enum class TextureFilteringQuality : unsigned int {
        LOW = 0,    //< Point filtering.
        MEDIUM = 1, //< Linear filtering.
        HIGH = 2    //< Anisotropic filtering.
    };

    /** Describes how much mipmaps we will skip when loading textures from disk. */
    enum class TextureQuality : unsigned int {
        VERY_HIGH = 0,
        HIGH = 1,
        MEDIUM = 2,
        LOW = 3,
    };

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
         * Sets the maximum number of FPS that is allowed to be produced in a second.
         *
         * @param iNewFpsLimit Maximum allowed FPS, specify 0 to disable.
         */
        void setFpsLimit(unsigned int iNewFpsLimit);

        /**
         * Sets shadow quality.
         *
         * @param quality Quality to use.
         */
        void setShadowQuality(ShadowQuality quality);

        /**
         * Sets anti-aliasing (AA) quality.
         *
         * @warning Use @ref getMaxSupportedAntialiasingQuality to query for maximum supported AA quality,
         * if the specified quality is not supported an error will be logged.
         *
         * @param quality AA quality.
         */
        void setAntialiasingQuality(AntialiasingQuality quality);

        /**
         * Sets texture filtering quality to be used.
         *
         * @param quality Quality to use.
         */
        void setTextureFilteringQuality(TextureFilteringQuality quality);

        /**
         * Sets texture quality to be used.
         *
         * @remark In order for this setting to be applied the engine needs to be restarted.
         *
         * @param quality Quality to use.
         */
        void setTextureQuality(TextureQuality quality);

        /**
         * Sets the width and the height of the render resolution.
         *
         * @remark Use `Renderer::getSupportedRenderResolutions` to query for available
         * render resolutions.
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
         * @remark In order for this setting to be applied the engine needs to be restarted.
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
         * Returns the maximum number of FPS that is allowed to be produced in a second.
         *
         * @return Maximum allowed FPS, 0 means disabled.
         */
        unsigned int getFpsLimit() const;

        /**
         * Returns current anti-aliasing (AA) quality.
         *
         * @return Returns `DISABLED` if AA is not supported (see @ref getMaxSupportedAntialiasingQuality),
         * otherwise current AA quality.
         */
        AntialiasingQuality getAntialiasingQuality() const;

        /**
         * Returns current shadow quality.
         *
         * @return Shadow quality.
         */
        ShadowQuality getShadowQuality() const;

        /**
         * Returns the maximum anti-aliasing quality that can be used on the picked
         * GPU (see `Renderer::getCurrentlyUsedGpuName`).
         *
         * @remark Note that the maximum supported AA quality can differ depending on the used GPU/renderer.
         *
         * @return Error if something went wrong,
         * otherwise `DISABLED` if AA is not supported or the maximum supported AA quality.
         */
        std::variant<AntialiasingQuality, Error> getMaxSupportedAntialiasingQuality() const;

        /**
         * Returns currently used texture filtering quality.
         *
         * @return Texture filtering quality.
         */
        TextureFilteringQuality getTextureFilteringQuality() const;

        /**
         * Returns currently used texture quality.
         *
         * @return Texture quality.
         */
        TextureQuality getTextureQuality() const;

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
         *
         * @param bShadowMapSizeChanged `true` if shadow map size was changed, `false` otherwise.
         */
        void notifyRendererAboutChangedSettings(bool bShadowMapSizeChanged = false);

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

        /**
         * The maximum number of FPS that is allowed to be produced in a second.
         *
         * @remark Disabled if zero.
         */
        RPROPERTY(Serialize)
        unsigned int iFpsLimit = 0;

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

        /** Shadow map resolution in pixels. */
        RPROPERTY(Serialize)
        unsigned int iShadowMapSize = static_cast<unsigned int>(ShadowQuality::HIGH);

        /** Sample count of AA. */
        RPROPERTY(Serialize)
        unsigned int iAntialiasingSampleCount = static_cast<unsigned int>(AntialiasingQuality::HIGH);

        /** Texture filtering mode. */
        RPROPERTY(Serialize)
        unsigned int iTextureFilteringQuality = static_cast<unsigned int>(TextureFilteringQuality::HIGH);

        /** Defines how much mipmaps we will skip when loading textures from disk. */
        RPROPERTY(Serialize)
        unsigned int iTextureQuality = static_cast<unsigned int>(TextureQuality::VERY_HIGH);

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
