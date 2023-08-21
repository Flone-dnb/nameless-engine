#include "render/RenderSettings.h"

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

    void RenderSettings::setAntialiasingState(MsaaState state) {
        const auto iNewSampleCount = static_cast<int>(state);
        if (iAntialiasingSampleCount == iNewSampleCount) {
            return; // do nothing
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
            Logger::get().error(fmt::format(
                "failed to set anti-aliasing sample count {} because it's not supported (max supported: {})",
                iNewSampleCount,
                iMaxSampleCount));
            return;
        }

        // Log change.
        Logger::get().info(fmt::format(
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
            Logger::get().error(fmt::format(
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
            Logger::get().error(fmt::format(
                "expected the current AA sample count {} to be supported",
                static_cast<int>(iAntialiasingSampleCount)));
        }

        return static_cast<MsaaState>(iAntialiasingSampleCount);
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
            Logger::get().warn(fmt::format(
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
            Logger::get().warn(fmt::format(
                "deserialized texture filtering mode \"{}\" is not a valid parameter, changing to \"{}\"",
                iTextureFilteringMode,
                iNewTextureFilteringMode));

            // Correct the value.
            iTextureFilteringMode = iNewTextureFilteringMode;
        }

#if defined(DEBUG)
        static_assert(
            sizeof(RenderSettings) == 184, "consider adding new checks here"); // NOLINT: current class size
#endif
    }

    void RenderSettings::setTextureFilteringMode(TextureFilteringMode mode) {
        if (iTextureFilteringMode == static_cast<int>(mode)) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(fmt::format(
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
            Logger::get().error(fmt::format(
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

        // Log change.
        Logger::get().info(fmt::format(
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
            Logger::get().error(fmt::format(
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
            fmt::format("VSync state is being changed from \"{}\" to \"{}\"", bIsVsyncEnabled, bEnableVsync));

        // Change.
        bIsVsyncEnabled = bEnableVsync;

        // Notify renderer.
        notifyRendererAboutChangedSettings();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            Logger::get().error(fmt::format(
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
        Logger::get().info(fmt::format(
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
            Logger::get().error(fmt::format(
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
        auto result = pRenderer->getSupportedGpuNames();
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addCurrentLocationToErrorStack();
            Logger::get().error(fmt::format(
                "failed to get the list of supported GPUs, error: \"{}\"", error.getFullErrorMessage()));
            return;
        }
        const auto vSupportedGpuNames = std::get<std::vector<std::string>>(std::move(result));

        // Make sure we fit into the `unsigned int` range.
        if (vSupportedGpuNames.size() > UINT_MAX) [[unlikely]] {
            Logger::get().error(fmt::format(
                "list of supported GPUs is too big to handle ({} items)", vSupportedGpuNames.size()));
            return;
        }

        // Find supported GPU by this name.
        const auto it = std::ranges::find(vSupportedGpuNames, sGpuName);
        if (it == vSupportedGpuNames.end()) {
            Logger::get().error(fmt::format(
                "failed to find the specified GPU \"{}\" in the list of supported GPUs",
                vSupportedGpuNames.size()));
            return;
        }

        if (!sGpuToUse.empty()) {
            // Log change if the previous GPU name was set.
            Logger::get().info(
                fmt::format("preferred GPU is being changed from \"{}\" to \"{}\"", sGpuToUse, sGpuName));
        }

        // Change.
        sGpuToUse = sGpuName;

        // Engine needs to be restarted in order for the setting to be applied.

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addCurrentLocationToErrorStack();
            Logger::get().error(fmt::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    void RenderSettings::notifyRendererAboutChangedSettings() {
        // Make sure the renderer is initialized.
        if (!pRenderer->isInitialized()) {
            // Nothing to do. The renderer will take values from the settings upon initialization.
            return;
        }

        // Notify renderer.
        auto optionalError = pRenderer->onRenderSettingsChanged();
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
        Logger::get().info(fmt::format(
            "preferred renderer is being changed from \"{}\" to \"{}\"", iRendererType, iNewRendererType));

        // Change.
        iRendererType = iNewRendererType;

        // Engine needs to be restarted in order for the setting to be applied.

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            optionalError->addCurrentLocationToErrorStack();
            Logger::get().error(fmt::format(
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
            Logger::get().info(fmt::format(
                "AA sample count \"{}\" is not supported, changing to \"{}\"",
                iAntialiasingSampleCount,
                iMaxSampleCount));

            iAntialiasingSampleCount = iMaxSampleCount;
        }

        return {};
    }

} // namespace ne
