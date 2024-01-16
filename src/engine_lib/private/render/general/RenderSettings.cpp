#include "render/RenderSettings.h"

// Standard.
#include <format>

// Custom.
#include "render/Renderer.h"
#include "render/general/pipeline/PipelineManager.h"
#include "io/Logger.h"
#include "misc/ProjectPaths.h"

#include "RenderSettings.generated_impl.h"

namespace ne {
    void RenderSettings::setRenderer(Renderer* pRenderer) { this->pRenderer = pRenderer; }

    std::filesystem::path RenderSettings::getPathToConfigurationFile() {
        return ProjectPaths::getPathToEngineConfigsDirectory() / getConfigurationFileName(true);
    }

    void RenderSettings::setFpsLimit(unsigned int iNewFpsLimit) {
        // Make sure the new setting is different.
        if (iFpsLimit == iNewFpsLimit) {
            return;
        }

        // Log change.
        Logger::get().info(std::format("FPS limit is being changed from {} to {}", iFpsLimit, iNewFpsLimit));

        // Change.
        iFpsLimit = iNewFpsLimit;

        // Notify renderer.
        notifyRendererAboutChangedSettings();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            Logger::get().error(std::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    void RenderSettings::setShadowQuality(ShadowQuality quality) {
        // Make sure the new setting is different.
        const auto iNewShadowMapSize = static_cast<unsigned int>(quality);
        if (iShadowMapSize == iNewShadowMapSize) {
            return;
        }

        // Log change.
        Logger::get().info(std::format(
            "shadow map size is being changed from \"{}\" to \"{}\"", iShadowMapSize, iNewShadowMapSize));

        // Change.
        iShadowMapSize = iNewShadowMapSize;

        // Notify renderer.
        notifyRendererAboutChangedSettings(true);

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            Logger::get().error(std::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    void RenderSettings::setAntialiasingState(MsaaState state) {
        // Make sure the new setting is different.
        const auto iNewSampleCount = static_cast<int>(state);
        if (iAntialiasingSampleCount == iNewSampleCount) {
            return;
        }

        // Make sure this quality is supported.
        auto result = pRenderer->getMaxSupportedAntialiasingQuality();
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();

            // Not a critical error.
            Logger::get().error(error.getFullErrorMessage());
            return;
        }
        auto maxState = std::get<MsaaState>(std::move(result));

        // Make sure AA is supported.
        if (maxState == MsaaState::DISABLED) {
            Logger::get().error("failed to set anti-aliasing quality because anti-aliasing is not supported");
            return;
        }
        int iMaxSampleCount = static_cast<int>(maxState);

        // Make sure this quality is supported.
        if (iNewSampleCount > iMaxSampleCount) [[unlikely]] {
            Logger::get().error(std::format(
                "failed to set anti-aliasing sample count {} because it's not supported (max supported: {})",
                iNewSampleCount,
                iMaxSampleCount));
            return;
        }

        // Log change.
        Logger::get().info(std::format(
            "AA sample count is being changed from \"{}\" to \"{}\"",
            iAntialiasingSampleCount,
            iNewSampleCount));

        // Change.
        iAntialiasingSampleCount = iNewSampleCount;

        // Notify renderer.
        notifyRendererAboutChangedSettings();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            Logger::get().error(std::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    MsaaState RenderSettings::getAntialiasingState() const {
        // Get max AA quality.
        auto result = pRenderer->getMaxSupportedAntialiasingQuality();
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();

            Logger::get().error(error.getFullErrorMessage());
            return {};
        }
        auto maxState = std::get<MsaaState>(std::move(result));

        // Make sure AA is supported.
        if (maxState == MsaaState::DISABLED) {
            return {};
        }

        // Self check: make sure current AA sample count is supported (should be because RenderSettings
        // are expected to store valid AA sample count).
        if (static_cast<int>(iAntialiasingSampleCount) > static_cast<int>(maxState)) [[unlikely]] {
            Logger::get().error(std::format(
                "expected the current AA sample count {} to be supported",
                static_cast<int>(iAntialiasingSampleCount)));
        }

        return static_cast<MsaaState>(iAntialiasingSampleCount);
    }

    ShadowQuality RenderSettings::getShadowQuality() const {
        return static_cast<ShadowQuality>(iShadowMapSize);
    }

    std::optional<Error> RenderSettings::saveConfigurationToDisk() {
        if (!bAllowSavingConfigurationToDisk) {
            return {};
        }

        // Save.
        auto optionalError = serialize(getPathToConfigurationFile(), false);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            return optionalError;
        }

        return {};
    }

    void RenderSettings::onAfterDeserialized() {
        Serializable::onAfterDeserialized();

        // Make sure that deserialized values are valid/supported.

        // Check anti-aliasing sample count.
        if (iAntialiasingSampleCount != static_cast<int>(MsaaState::DISABLED) &&
            iAntialiasingSampleCount != static_cast<int>(MsaaState::MEDIUM) &&
            iAntialiasingSampleCount != static_cast<int>(MsaaState::HIGH) &&
            iAntialiasingSampleCount != static_cast<int>(MsaaState::VERY_HIGH)) {
            // Set new AA sample count.
            const auto iNewSampleCount = static_cast<int>(MsaaState::VERY_HIGH);

            // Log change.
            Logger::get().warn(std::format(
                "deserialized AA sample count \"{}\" is not a valid/supported value, changing to \"{}\"",
                iAntialiasingSampleCount,
                iNewSampleCount));

            // Correct the value.
            iAntialiasingSampleCount = iNewSampleCount;
        }

        // Check texture filtering mode.
        if (iTextureFilteringMode != static_cast<int>(TextureFilteringMode::POINT) &&
            iTextureFilteringMode != static_cast<int>(TextureFilteringMode::LINEAR) &&
            iTextureFilteringMode != static_cast<int>(TextureFilteringMode::ANISOTROPIC)) {
            const auto iNewTextureFilteringMode = static_cast<int>(TextureFilteringMode::ANISOTROPIC);

            // Log change.
            Logger::get().warn(std::format(
                "deserialized texture filtering mode \"{}\" is not a valid parameter, changing to \"{}\"",
                iTextureFilteringMode,
                iNewTextureFilteringMode));

            // Correct the value.
            iTextureFilteringMode = iNewTextureFilteringMode;
        }

        // Check shadow map resolution.
        if (iShadowMapSize != static_cast<unsigned int>(ShadowQuality::LOW) &&
            iShadowMapSize != static_cast<unsigned int>(ShadowQuality::MEDIUM) &&
            iShadowMapSize != static_cast<unsigned int>(ShadowQuality::HIGH)) {
            const auto iNewShadowMapSize = static_cast<unsigned int>(ShadowQuality::HIGH);

            // Log change.
            Logger::get().warn(std::format(
                "deserialized shadow map size \"{}\" is not a valid parameter, changing to \"{}\"",
                iShadowMapSize,
                iNewShadowMapSize));

            // Correct the value.
            iShadowMapSize = iNewShadowMapSize;
        }

#if defined(DEBUG) && defined(WIN32)
        static_assert(
            sizeof(RenderSettings) == 200, // NOLINT: current class size
            "consider updating old / adding new checks here");
#elif defined(DEBUG)
        static_assert(
            sizeof(RenderSettings) == 176, // NOLINT: current class size
            "consider updating old / adding new checks here");
#endif
    }

    void RenderSettings::setTextureFilteringMode(TextureFilteringMode mode) {
        if (iTextureFilteringMode == static_cast<int>(mode)) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(std::format(
            "texture filtering mode is being changed from \"{}\" to \"{}\"",
            iTextureFilteringMode,
            static_cast<int>(mode)));

        // Change.
        iTextureFilteringMode = static_cast<int>(mode);

        // Notify renderer.
        notifyRendererAboutChangedSettings();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            Logger::get().error(std::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    TextureFilteringMode RenderSettings::getTextureFilteringMode() const {
        return static_cast<TextureFilteringMode>(iTextureFilteringMode);
    }

    std::pair<unsigned int, unsigned int> RenderSettings::getRenderResolution() const {
        return std::make_pair(iRenderResolutionWidth, iRenderResolutionHeight);
    }

    void RenderSettings::setRenderResolution(std::pair<unsigned int, unsigned int> resolution) {
        if (iRenderResolutionWidth == resolution.first && iRenderResolutionHeight == resolution.second) {
            return; // do nothing
        }

        // Make sure this resolution is supported.
        auto result = pRenderer->getSupportedRenderResolutions();
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();

            // Not a critical error.
            Logger::get().error(error.getFullErrorMessage());
            return;
        }
        auto supportedResolutions =
            std::get<std::set<std::pair<unsigned int, unsigned int>>>(std::move(result));

        // Make sure the specified resolution is supported.
        bool bFound = false;
        for (const auto& supportedResolution : supportedResolutions) {
            if (supportedResolution == resolution) {
                bFound = true;
                break;
            }
        }

        // Make sure this resolution was found.
        if (!bFound) [[unlikely]] {
            Logger::get().error(std::format(
                "failed to set render resolution {}x{} because it's not supported (read the docs on how to "
                "query supported render resolutions)",
                resolution.first,
                resolution.second));
            return;
        }

        // Log change.
        Logger::get().info(std::format(
            "render resolution is being changed from \"{}x{}\" to \"{}x{}\"",
            iRenderResolutionWidth,
            iRenderResolutionHeight,
            resolution.first,
            resolution.second));

        // Change.
        iRenderResolutionWidth = resolution.first;
        iRenderResolutionHeight = resolution.second;

        // Notify renderer.
        notifyRendererAboutChangedSettings();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            Logger::get().error(std::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    void RenderSettings::setVsyncEnabled(bool bEnableVsync) {
        if (bIsVsyncEnabled == bEnableVsync) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            std::format("VSync state is being changed from \"{}\" to \"{}\"", bIsVsyncEnabled, bEnableVsync));

        // Change.
        bIsVsyncEnabled = bEnableVsync;

        // Notify renderer.
        notifyRendererAboutChangedSettings();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            Logger::get().error(std::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    bool RenderSettings::isVsyncEnabled() const { return bIsVsyncEnabled; }

    void RenderSettings::setRefreshRate(std::pair<unsigned int, unsigned int> refreshRate) {
        if (iRefreshRateNumerator == refreshRate.first && iRefreshRateDenominator == refreshRate.second) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(std::format(
            "refresh rate is being changed from \"{}/{}\" to \"{}/{}\"",
            iRefreshRateNumerator,
            iRefreshRateDenominator,
            refreshRate.first,
            refreshRate.second));

        // Change.
        iRefreshRateNumerator = refreshRate.first;
        iRefreshRateDenominator = refreshRate.second;

        // TODO: need to change without restarting
        // Notify renderer.
        // notifyRendererAboutChangedSettings();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            Logger::get().error(std::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    std::pair<unsigned int, unsigned int> RenderSettings::getRefreshRate() const {
        return std::make_pair(iRefreshRateNumerator, iRefreshRateDenominator);
    }

    void RenderSettings::setGpuToUse(const std::string& sGpuName) {
        if (sGpuToUse == sGpuName) {
            return; // do nothing
        }

        // Get names of supported GPUs.
        const auto vSupportedGpuNames = pRenderer->getSupportedGpuNames();

        // Make sure we fit into the `unsigned int` range.
        if (vSupportedGpuNames.size() > UINT_MAX) [[unlikely]] {
            Logger::get().error(std::format(
                "list of supported GPUs is too big to handle ({} items)", vSupportedGpuNames.size()));
            return;
        }

        // Find supported GPU by this name.
        const auto it = std::ranges::find(vSupportedGpuNames, sGpuName);
        if (it == vSupportedGpuNames.end()) {
            Logger::get().error(std::format(
                "failed to find the specified GPU \"{}\" in the list of supported GPUs",
                vSupportedGpuNames.size()));
            return;
        }

        if (!sGpuToUse.empty()) {
            // Log change if the previous GPU name was set.
            Logger::get().info(
                std::format("preferred GPU is being changed from \"{}\" to \"{}\"", sGpuToUse, sGpuName));
        }

        // Change.
        sGpuToUse = sGpuName;

        // Engine needs to be restarted in order for the setting to be applied.

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            Logger::get().error(std::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    void RenderSettings::notifyRendererAboutChangedSettings(bool bShadowMapSizeChanged) {
        // Make sure the renderer is initialized.
        if (!pRenderer->isInitialized()) {
            // Nothing to do. The renderer will take values from the settings upon initialization.
            return;
        }

        // Notify renderer.
        auto optionalError = pRenderer->onRenderSettingsChanged(bShadowMapSizeChanged);
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }
    }

    std::string RenderSettings::getConfigurationFileName(bool bIncludeFileExtension) {
        if (bIncludeFileExtension) {
            return sRenderSettingsConfigurationFileName + ConfigManager::getConfigFormatExtension();
        }

        return sRenderSettingsConfigurationFileName;
    }

    void RenderSettings::setPreferredRenderer(RendererType preferredRenderer) {
        const auto iNewRendererType = static_cast<unsigned int>(preferredRenderer);

        if (iNewRendererType == iRendererType) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(std::format(
            "preferred renderer is being changed from \"{}\" to \"{}\"", iRendererType, iNewRendererType));

        // Change.
        iRendererType = iNewRendererType;

        // Engine needs to be restarted in order for the setting to be applied.

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            Logger::get().error(std::format(
                "failed to save new render setting configuration, error: \"{}\"",
                optionalError->getFullErrorMessage()));
        }
    }

    std::string RenderSettings::getGpuToUse() const { return sGpuToUse; }

    std::variant<MsaaState, Error> RenderSettings::getMaxSupportedAntialiasingQuality() const {
        return pRenderer->getMaxSupportedAntialiasingQuality();
    }

    std::optional<Error> RenderSettings::clampSettingsToMaxSupported() {
        // Get maximum supported AA sample count.
        auto result = pRenderer->getMaxSupportedAntialiasingQuality();
        if (std::holds_alternative<Error>(result)) [[unlikely]] {
            auto error = std::get<Error>(result);
            error.addCurrentLocationToErrorStack();
            return error;
        }

        const auto iMaxSampleCount = static_cast<int>(std::get<MsaaState>(result));

        // Clamp sample count to max value.
        if (iAntialiasingSampleCount > iMaxSampleCount) {
            // Log change.
            Logger::get().info(std::format(
                "AA sample count \"{}\" is not supported, changing to \"{}\"",
                iAntialiasingSampleCount,
                iMaxSampleCount));

            iAntialiasingSampleCount = iMaxSampleCount;
        }

        return {};
    }

    unsigned int RenderSettings::getFpsLimit() const { return iFpsLimit; }

} // namespace ne
