#pragma once

// Custom.
#include "io/Serializable.h"

#include "RenderSettings.generated.h"

namespace ne RNAMESPACE() {
    class Renderer;

    /** Describes "types" of derived Renderer classes. */
    enum class RendererType : unsigned int { DIRECTX = 0, VULKAN = 1 };

    /** Describes the quality (sample count) of MSAA. */
    enum class MsaaQuality : int { MEDIUM = 2, HIGH = 4 };

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
         * Enables/disables anti-aliasing (AA).
         *
         * @param bEnable Whether to enable AA or not.
         */
        void setAntialiasingEnabled(bool bEnable);

        /**
         * Sets anti-aliasing (AA) quality.
         *
         * @param quality AA quality.
         */
        void setAntialiasingQuality(MsaaQuality quality);

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
         * Sets GPU to be used next time the engine starts.
         *
         * @remark In order for this setting to be applied the engine needs to be restarted.
         *
         * @param iGpuIndex Index of the GPU to use.
         */
        void setGpuToUse(unsigned int iGpuIndex);

        /**
         * Tells whether anti-aliasing (AA) is currently enabled or not.
         *
         * @return Whether AA is enabled or not.
         */
        bool isAntialiasingEnabled() const;

        /**
         * Returns current anti-aliasing (AA) quality.
         *
         * @return Current AA quality.
         */
        MsaaQuality getAntialiasingQuality() const;

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
         * Saves the current configuration to disk.
         *
         * @remark Does nothing if @ref bAllowSavingConfigurationToDisk is `false`.
         *
         * @return Error if something went wrong.
         */
        std::optional<Error> saveConfigurationToDisk();

        /**
         * Changes renderer's state to match the current settings.
         *
         * @remark Does nothing if the renderer is not initialized yet. The renderer
         * will take values from the settings upon initialization.
         */
        void updateRendererConfiguration();

        /** Changes renderer's state to match the current texture filtering settings. */
        void updateRendererConfigurationForTextureFiltering();

        /** Changes renderer' state to match the current antialiasing settings. */
        void updateRendererConfigurationForAntialiasing();

        /** Changes renderer's state to match the current screen settings. */
        void updateRendererConfigurationForScreen();

        // !!!
        // !!! don't forget to add new `updateRendererConfiguration...` function to
        // !!! `updateRendererConfiguration()` function.
        // !!!

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

        /**
         * Stored used type of the renderer (DirectX/Vulkan/etc.).
         *
         * @remark Can be changed from the config file to change preferred renderer.
         */
        RPROPERTY(Serialize)
        unsigned int iRendererType = 0;

        /** Sample count of AA. */
        RPROPERTY(Serialize)
        int iAntialiasingSampleCount = static_cast<int>(MsaaQuality::HIGH);

        /** Texture filtering mode. */
        RPROPERTY(Serialize)
        int iTextureFilteringMode = static_cast<int>(TextureFilteringMode::ANISOTROPIC);

        /** Whether VSync is enabled or not. */
        RPROPERTY(Serialize)
        bool bIsVsyncEnabled = false;

        /** Whether AA is enabled or not. */
        RPROPERTY(Serialize)
        bool bIsAntialiasingEnabled = true;

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
