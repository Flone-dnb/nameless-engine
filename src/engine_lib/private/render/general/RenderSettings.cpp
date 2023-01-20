#include "render/RenderSettings.h"

#include "render/Renderer.h"
#include "render/general/pso/PsoManager.h"
#include "io/Logger.h"
#include "misc/ProjectPaths.h"

namespace ne {
    void RenderSettings::setRenderer(Renderer* pRenderer) { this->pRenderer = pRenderer; }

    std::filesystem::path RenderSettings::getPathToConfigurationFile() {
        return ProjectPaths::getDirectoryForEngineConfigurationFiles() / getConfigurationFileName(true);
    }

    void RenderSettings::setAntialiasingEnabled(bool bEnable) {
        if (bIsAntialiasingEnabled == bEnable) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format("AA state is being changed from \"{}\" to \"{}\"", bIsAntialiasingEnabled, bEnable),
            sRenderSettingsLogCategory);

        // Change.
        bIsAntialiasingEnabled = bEnable;

        // Update.
        updateRendererConfigurationForAntialiasing();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"",
                    error.getFullErrorMessage()),
                sRenderSettingsLogCategory);
        }
    }

    bool RenderSettings::isAntialiasingEnabled() const { return bIsAntialiasingEnabled; }

    void RenderSettings::setAntialiasingQuality(MsaaQuality quality) {
        if (iAntialiasingSampleCount == static_cast<int>(quality)) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format(
                "AA sample count is being changed from \"{}\" to \"{}\"",
                iAntialiasingSampleCount,
                static_cast<int>(quality)),
            sRenderSettingsLogCategory);

        // Change.
        iAntialiasingSampleCount = static_cast<int>(quality);

        // Update.
        updateRendererConfigurationForAntialiasing();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"",
                    error.getFullErrorMessage()),
                sRenderSettingsLogCategory);
        }
    }

    void RenderSettings::updateRendererConfigurationForAntialiasing() {
        if (pRenderer->isInitialized()) {
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
    }

    MsaaQuality RenderSettings::getAntialiasingQuality() const {
        return static_cast<MsaaQuality>(iAntialiasingSampleCount);
    }

    std::optional<Error> RenderSettings::saveConfigurationToDisk() {
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
            Logger::get().warn(
                std::format(
                    "deserialized AA quality \"{}\" is not a valid parameter, changing to \"{}\"",
                    iAntialiasingSampleCount,
                    iNewSampleCount),
                sRenderSettingsLogCategory);

            // Correct the value.
            iAntialiasingSampleCount = iNewSampleCount;
        }

        // Check texture filtering mode.
        if (iTextureFilteringMode != static_cast<int>(TextureFilteringMode::POINT) &&
            iTextureFilteringMode != static_cast<int>(TextureFilteringMode::LINEAR) &&
            iTextureFilteringMode != static_cast<int>(TextureFilteringMode::ANISOTROPIC)) {
            const auto iNewTextureFilteringMode = static_cast<int>(TextureFilteringMode::ANISOTROPIC);

            // Log change.
            Logger::get().warn(
                std::format(
                    "deserialized texture filtering mode \"{}\" is not a valid parameter, changing to \"{}\"",
                    iTextureFilteringMode,
                    iNewTextureFilteringMode),
                sRenderSettingsLogCategory);

            // Correct the value.
            iTextureFilteringMode = iNewTextureFilteringMode;
        }
    }

    void RenderSettings::setTextureFilteringMode(TextureFilteringMode mode) {
        if (iTextureFilteringMode == static_cast<int>(mode)) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format(
                "texture filtering mode is being changed from \"{}\" to \"{}\"",
                iTextureFilteringMode,
                static_cast<int>(mode)),
            sRenderSettingsLogCategory);

        // Change.
        iTextureFilteringMode = static_cast<int>(mode);

        // Update.
        updateRendererConfigurationForTextureFiltering();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"",
                    error.getFullErrorMessage()),
                sRenderSettingsLogCategory);
        }
    }

    void RenderSettings::updateRendererConfigurationForTextureFiltering() {
        // Change shader configuration.
        const auto pShaderConfiguration = pRenderer->getShaderConfiguration();

        {
            std::scoped_lock guard(pShaderConfiguration->first);

            // First, remove all settings.
            pShaderConfiguration->second->currentPixelShaderConfiguration.erase(
                ShaderParameter::TEXTURE_FILTERING_POINT);
            pShaderConfiguration->second->currentPixelShaderConfiguration.erase(
                ShaderParameter::TEXTURE_FILTERING_LINEAR);
            pShaderConfiguration->second->currentPixelShaderConfiguration.erase(
                ShaderParameter::TEXTURE_FILTERING_ANISOTROPIC);

            // Now add the appropriate one.
            switch (static_cast<TextureFilteringMode>(iTextureFilteringMode)) {
            case (TextureFilteringMode::POINT): {
                pShaderConfiguration->second->currentPixelShaderConfiguration.insert(
                    ShaderParameter::TEXTURE_FILTERING_POINT);
                break;
            }
            case (TextureFilteringMode::LINEAR): {
                pShaderConfiguration->second->currentPixelShaderConfiguration.insert(
                    ShaderParameter::TEXTURE_FILTERING_LINEAR);
                break;
            }
            case (TextureFilteringMode::ANISOTROPIC): {
                pShaderConfiguration->second->currentPixelShaderConfiguration.insert(
                    ShaderParameter::TEXTURE_FILTERING_ANISOTROPIC);
                break;
            }
            }

            // Update configuration in renderer.
            pShaderConfiguration->second->updateShaderConfiguration();
        }
    }

    TextureFilteringMode RenderSettings::getTextureFilteringMode() const {
        return static_cast<TextureFilteringMode>(iTextureFilteringMode);
    }

    std::pair<unsigned int, unsigned int> RenderSettings::getRenderResolution() {
        return std::make_pair(iRenderResolutionWidth, iRenderResolutionHeight);
    }

    void RenderSettings::setRenderResolution(std::pair<unsigned int, unsigned int> resolution) {
        if (iRenderResolutionWidth == resolution.first && iRenderResolutionHeight == resolution.second) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format(
                "render resolution is being changed from \"{}x{}\" to \"{}x{}\"",
                iRenderResolutionWidth,
                iRenderResolutionHeight,
                resolution.first,
                resolution.second),
            sRenderSettingsLogCategory);

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
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"",
                    error.getFullErrorMessage()),
                sRenderSettingsLogCategory);
        }
    }

    void RenderSettings::setVsyncEnabled(bool bEnableVsync) {
        if (bIsVsyncEnabled == bEnableVsync) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format("vsync state is being changed from \"{}\" to \"{}\"", bIsVsyncEnabled, bEnableVsync),
            sRenderSettingsLogCategory);

        // Change.
        bIsVsyncEnabled = bEnableVsync;

        // Update.
        updateRendererConfigurationForScreen();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"",
                    error.getFullErrorMessage()),
                sRenderSettingsLogCategory);
        }
    }

    void RenderSettings::updateRendererConfigurationForScreen() {
        if (pRenderer->isInitialized()) {
            // Update render buffers.
            auto optionalError = pRenderer->updateRenderBuffers();
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addEntry();
                error.showError();
                throw std::runtime_error(error.getFullErrorMessage());
            }
        }
    }

    bool RenderSettings::isVsyncEnabled() const { return bIsVsyncEnabled; }

    void RenderSettings::setRefreshRate(std::pair<unsigned int, unsigned int> refreshRate) {
        if (iRefreshRateNumerator == refreshRate.first && iRefreshRateDenominator == refreshRate.second) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format(
                "refresh rate is being changed from \"{}/{}\" to \"{}/{}\"",
                iRefreshRateNumerator,
                iRefreshRateDenominator,
                refreshRate.first,
                refreshRate.second),
            sRenderSettingsLogCategory);

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
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"",
                    error.getFullErrorMessage()),
                sRenderSettingsLogCategory);
        }
    }

    std::pair<unsigned int, unsigned int> RenderSettings::getRefreshRate() const {
        return std::make_pair(iRefreshRateNumerator, iRefreshRateDenominator);
    }

    void RenderSettings::setGpuToUse(const std::string& sGpuName) {
        if (sGpuName == getUsedGpuName()) {
            return; // do nothing
        }

        // Get names of supported GPUs.
        auto result = pRenderer->getSupportedGpuNames();
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to get the list of supported GPUs, error: \"{}\"", error.getFullErrorMessage()),
                sRenderSettingsLogCategory);
            return;
        }
        const auto vSupportedGpuNames = std::get<std::vector<std::string>>(std::move(result));

        // Make sure we fit into the `unsigned int` range.
        if (vSupportedGpuNames.size() > UINT_MAX) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "list of supported GPUs is too big to handle ({} items)", vSupportedGpuNames.size()),
                sRenderSettingsLogCategory);
            return;
        }

        // Find the specified GPU.
        for (unsigned int i = 0; i < static_cast<unsigned int>(vSupportedGpuNames.size()); i++) {
            if (vSupportedGpuNames[i] == sGpuName) {
                // Log change.
                Logger::get().info(
                    fmt::format(
                        "preferred GPU is being changed from \"{}\" to \"{}\"",
                        vSupportedGpuNames[iUsedGpuIndex],
                        sGpuName),
                    sRenderSettingsLogCategory);

                // Change.
                iUsedGpuIndex = i;

                // Engine needs to be restarted in order for the setting to be applied.

                // Save.
                auto optionalError = saveConfigurationToDisk();
                if (optionalError.has_value()) {
                    auto error = optionalError.value();
                    error.addEntry();
                    Logger::get().error(
                        fmt::format(
                            "failed to save new render setting configuration, error: \"{}\"",
                            error.getFullErrorMessage()),
                        sRenderSettingsLogCategory);
                }
                return;
            }
        }

        // Log.
        Logger::get().error(
            fmt::format("failed to find the GPU \"{}\" in the list of supported GPUs", sGpuName),
            sRenderSettingsLogCategory);
    }

    void RenderSettings::setGpuToUse(unsigned int iGpuIndex) {
        if (iGpuIndex == iUsedGpuIndex) {
            return; // do nothing
        }

        // Get names of supported GPUs.
        auto result = pRenderer->getSupportedGpuNames();
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to get the list of supported GPUs, error: \"{}\"", error.getFullErrorMessage()),
                sRenderSettingsLogCategory);
            return;
        }
        const auto vSupportedGpuNames = std::get<std::vector<std::string>>(std::move(result));

        // Make sure we fit into the `unsigned int` range.
        if (vSupportedGpuNames.size() > UINT_MAX) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "list of supported GPUs is too big to handle ({} items)", vSupportedGpuNames.size()),
                sRenderSettingsLogCategory);
            return;
        }

        // Make sure this index is in range.
        if (iGpuIndex >= static_cast<unsigned int>(vSupportedGpuNames.size())) {
            Logger::get().error(
                fmt::format(
                    "specified GPU index to use ({}) is out of range, supported GPUs in total: {}",
                    iGpuIndex,
                    vSupportedGpuNames.size()),
                sRenderSettingsLogCategory);
            return;
        }

        // Log change.
        Logger::get().info(
            fmt::format(
                "preferred GPU is being changed from \"{}\" to \"{}\"",
                vSupportedGpuNames[iUsedGpuIndex],
                vSupportedGpuNames[iGpuIndex]),
            sRenderSettingsLogCategory);

        // Change.
        iUsedGpuIndex = iGpuIndex;

        // Engine needs to be restarted in order for the setting to be applied.

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"",
                    error.getFullErrorMessage()),
                sRenderSettingsLogCategory);
        }
    }

    std::string RenderSettings::getUsedGpuName() const { return pRenderer->getCurrentlyUsedGpuName(); }

    unsigned int RenderSettings::getUsedGpuIndex() const { return iUsedGpuIndex; }

    void RenderSettings::updateRendererConfiguration() {
        updateRendererConfigurationForAntialiasing();
        updateRendererConfigurationForTextureFiltering();
        updateRendererConfigurationForScreen();
    }

    std::string RenderSettings::getConfigurationFileName(bool bIncludeFileExtension) {
        if (bIncludeFileExtension) {
            return sRenderSettingsConfigurationFileName + ConfigManager::getConfigFormatExtension();
        } else {
            return sRenderSettingsConfigurationFileName;
        }
    }

} // namespace ne
