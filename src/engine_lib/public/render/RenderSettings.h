#pragma once

// Custom.
#include "io/Serializable.h"
#include "materials/ShaderParameter.h"
#include "materials/ShaderDescription.h"

#include "RenderSettings.generated.h"

namespace ne RNAMESPACE() {
    class Renderer;

    /** Describes the quality (sample count) of MSAA. */
    enum class MsaaQuality : int { MEDIUM = 2, HIGH = 4 };

    /** Describes texture filtering mode. */
    enum class TextureFilteringMode : int { POINT = 0, LINEAR = 1, ANISOTROPIC = 2 };

    /**
     * Base class for render settings.
     *
     * @remark In order to create an object of this class you need to make sure that both game and renderer
     * objects exist.
     */
    class RCLASS(Guid("eb477c6d-cdc4-4b7a-9349-296fb38e6bfc")) RenderSettings : public Serializable {
    public:
        RenderSettings() = default;
        virtual ~RenderSettings() override = default;

        /**
         * Returns path to the file that is used to store setting configuration.
         *
         * @return Path to file.
         */
        std::filesystem::path getPathToConfigurationFile();

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
         * Sets texture filtering mode to be used.
         *
         * @param mode Mode to use.
         */
        void setTextureFilteringMode(TextureFilteringMode mode);

        /**
         * Returns currently used texture filtering mode.
         *
         * @return Texture filtering mode.
         */
        TextureFilteringMode getTextureFilteringMode() const;

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
         * Called after the object was successfully deserialized.
         * Used to execute post-deserialization logic.
         *
         * @warning If overriding you must call the parent's version of this function first
         * (before executing your login) to execute parent's logic.
         */
        virtual void onAfterDeserialized() override;

    private:
        // Renderer will initialize this object.
        friend class Renderer;

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

        /** Changes renderer's state to match the current settings. */
        void updateRendererConfiguration();

        /**
         * Saves the current configuration to disk.
         *
         * @return Error if something went wrong.
         */
        [[nodiscard]] std::optional<Error> saveConfigurationToDisk();

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

        /** Name of the file we use to store render settings. */
        static inline const char* sRenderSettingsConfigurationFileName = "render";

        /** Name of the category used for logging. */
        static inline const char* sRenderSettingsLogCategory = "Render Settings";

        ne_RenderSettings_GENERATED
    };
} // namespace ne RNAMESPACE()

File_RenderSettings_GENERATED
