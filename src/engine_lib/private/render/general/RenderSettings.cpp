#include "render/RenderSettings.h"

#include "render/Renderer.h"
#include "render/general/pso/PsoManager.h"
#include "io/Logger.h"
#include "misc/ProjectPaths.h"

#include "RenderSettings.generated_impl.h"

namespace ne {
    void RenderSettings::setRenderer(Renderer* pRenderer) { this->pRenderer = pRenderer; }

    std::filesystem::path RenderSettings::getPathToConfigurationFile() {
        return ProjectPaths::getPathToEngineConfigsDirectory() / getConfigurationFileName(true);
    }

    void RenderSettings::setAntialiasingEnabled(bool bEnable) {
        if (bIsAntialiasingEnabled == bEnable) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format("AA state is being changed from \"{}\" to \"{}\"", bIsAntialiasingEnabled, bEnable));

        // Change.
        bIsAntialiasingEnabled = bEnable;

        // Update.
        updateRendererConfigurationForAntialiasing();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(fmt::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    bool RenderSettings::isAntialiasingEnabled() const { return bIsAntialiasingEnabled; }

    void RenderSettings::setAntialiasingQuality(MsaaQuality quality) {
        if (iAntialiasingSampleCount == static_cast<int>(quality)) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(fmt::format(
            "AA sample count is being changed from \"{}\" to \"{}\"",
            iAntialiasingSampleCount,
            static_cast<int>(quality)));

        // Change.
        iAntialiasingSampleCount = static_cast<int>(quality);

        // Update.
        updateRendererConfigurationForAntialiasing();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(fmt::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    void RenderSettings::updateRendererConfigurationForAntialiasing() {
        if (!pRenderer->isInitialized()) {
            return;
        }

        // Make sure no drawing is happening and the GPU is not referencing any resources.
        std::scoped_lock guard(*pRenderer->getRenderResourcesMutex());
        pRenderer->waitForGpuToFinishWorkUpToThisPoint();

        // Recreate depth/stencil buffer with(out) multisampling.
        auto optionalError = pRenderer->updateRenderBuffers();
        if (optionalError.has_value()) {
            optionalError->addEntry();
            optionalError->showError();
            throw std::runtime_error(optionalError->getFullErrorMessage());
        }

        // Recreate all PSOs' internal resources so they will now use new multisampling settings.
        {
            const auto psoGuard =
                pRenderer->getPsoManager()->clearGraphicsPsosInternalResourcesAndDelayRestoring();
        }
    }

    MsaaQuality RenderSettings::getAntialiasingQuality() const {
        return static_cast<MsaaQuality>(iAntialiasingSampleCount);
    }

    std::optional<Error> RenderSettings::saveConfigurationToDisk() {
        if (!bAllowSavingConfigurationToDisk) {
            return {};
        }

        // Save.
        auto optionalError = serialize(getPathToConfigurationFile(), false);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    void RenderSettings::onAfterDeserialized() {
        Serializable::onAfterDeserialized();

        // Make sure that deserialized values are valid.

        // Check antialiasing quality.
        if (iAntialiasingSampleCount != static_cast<int>(MsaaQuality::MEDIUM) &&
            iAntialiasingSampleCount != static_cast<int>(MsaaQuality::HIGH)) {
            const auto iNewSampleCount = static_cast<int>(MsaaQuality::HIGH);

            // Log change.
            Logger::get().warn(std::format(
                "deserialized AA quality \"{}\" is not a valid parameter, changing to \"{}\"",
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

        // Update.
        updateRendererConfigurationForTextureFiltering();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(fmt::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    void RenderSettings::updateRendererConfigurationForTextureFiltering() {
        if (!pRenderer->isInitialized()) {
            return;
        }

        // This will recreate internal PSO resources and they will use new texture filtering setting:
        const auto psoGuard =
            pRenderer->getPsoManager()->clearGraphicsPsosInternalResourcesAndDelayRestoring();
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

        // Update.
        updateRendererConfigurationForScreen();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
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
            fmt::format("vsync state is being changed from \"{}\" to \"{}\"", bIsVsyncEnabled, bEnableVsync));

        // Change.
        bIsVsyncEnabled = bEnableVsync;

        // Update.
        updateRendererConfigurationForScreen();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(fmt::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    void RenderSettings::updateRendererConfigurationForScreen() {
        if (!pRenderer->isInitialized()) {
            return;
        }

        // Update render buffers.
        auto optionalError = pRenderer->updateRenderBuffers();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
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
        // updateRendererConfigurationForScreen();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
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
            error.addEntry();
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
            error.addEntry();
            Logger::get().error(fmt::format(
                "failed to save new render setting configuration, error: \"{}\"",
                error.getFullErrorMessage()));
        }
    }

    void RenderSettings::updateRendererConfiguration() {
        updateRendererConfigurationForAntialiasing();
        updateRendererConfigurationForTextureFiltering();
        updateRendererConfigurationForScreen();

#if defined(DEBUG)
        static_assert(
            sizeof(RenderSettings) == 184, "add new render settings here"); // NOLINT: current class size
#endif
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
            optionalError->addEntry();
            Logger::get().error(fmt::format(
                "failed to save new render setting configuration, error: \"{}\"",
                optionalError->getFullErrorMessage()));
        }
    }

    std::string RenderSettings::getGpuToUse() const { return sGpuToUse; }

} // namespace ne
