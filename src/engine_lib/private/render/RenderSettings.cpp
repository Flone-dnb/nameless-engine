#include "render/RenderSettings.h"

#include "render/Renderer.h"
#include "render/pso/PsoManager.h"
#include "io/Logger.h"
#include "misc/ProjectPaths.h"

namespace ne {
    void RenderSetting::setRenderer(Renderer* pRenderer) { this->pRenderer = pRenderer; }

    std::optional<Error> RenderSetting::updateRenderBuffers() { return getRenderer()->updateRenderBuffers(); }

    Renderer* RenderSetting::getRenderer() const { return pRenderer; }

    bool RenderSetting::isRendererInitialized() const { return pRenderer->isInitialized(); }

    const char* RenderSetting::getRenderSettingLogCategory() { return sRenderSettingLogCategory; }

    void AntialiasingRenderSetting::setEnabled(bool bEnable) {
        if (bIsEnabled == bEnable) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format("AA state is being changed from \"{}\" to \"{}\"", bIsEnabled, bEnable),
            getRenderSettingLogCategory());

        // Change.
        bIsEnabled = bEnable;

        // Update.
        updateRendererConfiguration();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"", error.getError()),
                getRenderSettingLogCategory());
        }
    }

    bool AntialiasingRenderSetting::isEnabled() const { return bIsEnabled; }

    void AntialiasingRenderSetting::setQuality(MsaaQuality quality) {
        if (iSampleCount == static_cast<int>(quality)) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format(
                "AA sample count is being changed from \"{}\" to \"{}\"",
                iSampleCount,
                static_cast<int>(quality)),
            getRenderSettingLogCategory());

        // Change.
        iSampleCount = static_cast<int>(quality);

        // Update.
        updateRendererConfiguration();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"", error.getError()),
                getRenderSettingLogCategory());
        }
    }

    void AntialiasingRenderSetting::updateRendererConfiguration() {
        const auto pRenderer = getRenderer();

        if (isRendererInitialized()) {
            // Make sure no drawing is happening and the GPU is not referencing any resources.
            std::scoped_lock guard(*pRenderer->getRenderResourcesMutex());
            pRenderer->flushCommandQueue();

            // Recreate depth/stencil buffer with(out) multisampling.
            auto optionalError = updateRenderBuffers();
            if (optionalError.has_value()) {
                optionalError->addEntry();
                optionalError->showError();
                throw std::runtime_error(optionalError->getError());
            }

            // Recreate all PSOs' internal resources so they will now use new multisampling settings.
            {
                const auto psoGuard =
                    pRenderer->getPsoManager()->clearGraphicsPsosInternalResourcesAndDelayRestoring();
            }
        }
    }

    MsaaQuality AntialiasingRenderSetting::getQuality() const {
        return static_cast<MsaaQuality>(iSampleCount);
    }

    std::optional<Error> AntialiasingRenderSetting::saveConfigurationToDisk() {
        auto optionalError = serialize(getPathToConfigurationFile(), false);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    void AntialiasingRenderSetting::onAfterDeserialized() {
        Serializable::onAfterDeserialized();

        // Make sure that deserialized values are valid.

        // Check quality.
        if (iSampleCount != static_cast<int>(MsaaQuality::MEDIUM) &&
            iSampleCount != static_cast<int>(MsaaQuality::HIGH)) {
            const auto iNewSampleCount = static_cast<int>(MsaaQuality::HIGH);

            // Log change.
            Logger::get().warn(
                std::format(
                    "deserialized AA quality \"{}\" is not a valid parameter, changing to \"{}\"",
                    iSampleCount,
                    iNewSampleCount),
                getRenderSettingLogCategory());

            // Correct the value.
            iSampleCount = iNewSampleCount;
        }
    }

    std::filesystem::path AntialiasingRenderSetting::getPathToConfigurationFile() {
        return ProjectPaths::getDirectoryForEngineConfigurationFiles() /
               getRenderer()->getRenderConfigurationDirectoryName() / getConfigurationFileName(true);
    }

    std::string AntialiasingRenderSetting::getConfigurationFileName(bool bIncludeFileExtension) {
        if (bIncludeFileExtension) {
            return sAntialiasingConfigFileName + ConfigManager::getConfigFormatExtension();
        } else {
            return sAntialiasingConfigFileName;
        }
    }

    std::filesystem::path TextureFilteringRenderSetting::getPathToConfigurationFile() {
        return ProjectPaths::getDirectoryForEngineConfigurationFiles() /
               getRenderer()->getRenderConfigurationDirectoryName() / getConfigurationFileName(true);
    }

    std::string TextureFilteringRenderSetting::getConfigurationFileName(bool bIncludeFileExtension) {
        if (bIncludeFileExtension) {
            return sTextureFilteringConfigFileName + ConfigManager::getConfigFormatExtension();
        } else {
            return sTextureFilteringConfigFileName;
        }
    }

    void TextureFilteringRenderSetting::onAfterDeserialized() {
        Serializable::onAfterDeserialized();

        // Make sure that deserialized values are valid.

        // Check filtering mode.
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
                getRenderSettingLogCategory());

            // Correct the value.
            iTextureFilteringMode = iNewTextureFilteringMode;
        }
    }

    std::optional<Error> TextureFilteringRenderSetting::saveConfigurationToDisk() {
        auto optionalError = serialize(getPathToConfigurationFile(), false);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    void TextureFilteringRenderSetting::setMode(TextureFilteringMode mode) {
        if (iTextureFilteringMode == static_cast<int>(mode)) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format(
                "texture filtering mode is being changed from \"{}\" to \"{}\"",
                iTextureFilteringMode,
                static_cast<int>(mode)),
            getRenderSettingLogCategory());

        // Change.
        iTextureFilteringMode = static_cast<int>(mode);

        // Update.
        updateRendererConfiguration();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"", error.getError()),
                getRenderSettingLogCategory());
        }
    }

    void TextureFilteringRenderSetting::updateRendererConfiguration() {
        const auto pRenderer = getRenderer();

        // Change shader configuration.
        {
            const auto pShaderConfiguration = pRenderer->getShaderConfiguration();

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

    TextureFilteringMode TextureFilteringRenderSetting::getMode() const {
        return static_cast<TextureFilteringMode>(iTextureFilteringMode);
    }

    std::optional<Error> ScreenRenderSetting::saveConfigurationToDisk() {
        auto optionalError = serialize(getPathToConfigurationFile(), false);
        if (optionalError.has_value()) {
            optionalError->addEntry();
            return optionalError;
        }

        return {};
    }

    std::string ScreenRenderSetting::getConfigurationFileName(bool bIncludeFileExtension) {
        if (bIncludeFileExtension) {
            return sScreenConfigFileName + ConfigManager::getConfigFormatExtension();
        } else {
            return sScreenConfigFileName;
        }
    }

    std::filesystem::path ScreenRenderSetting::getPathToConfigurationFile() {
        return ProjectPaths::getDirectoryForEngineConfigurationFiles() /
               getRenderer()->getRenderConfigurationDirectoryName() / getConfigurationFileName(true);
    }

    std::pair<unsigned int, unsigned int> ScreenRenderSetting::getRenderResolution() {
        return std::make_pair(iRenderResolutionWidth, iRenderResolutionHeight);
    }

    void ScreenRenderSetting::setRenderResolution(std::pair<unsigned int, unsigned int> resolution) {
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
            getRenderSettingLogCategory());

        // Change.
        iRenderResolutionWidth = resolution.first;
        iRenderResolutionHeight = resolution.second;

        // Update.
        updateRendererConfiguration();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"", error.getError()),
                getRenderSettingLogCategory());
        }
    }

    void ScreenRenderSetting::setVsyncEnabled(bool bEnableVsync) {
        if (bIsVsyncEnabled == bEnableVsync) {
            return; // do nothing
        }

        // Log change.
        Logger::get().info(
            fmt::format("vsync state is being changed from \"{}\" to \"{}\"", bIsVsyncEnabled, bEnableVsync),
            getRenderSettingLogCategory());

        // Change.
        bIsVsyncEnabled = bEnableVsync;

        // Update.
        updateRendererConfiguration();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"", error.getError()),
                getRenderSettingLogCategory());
        }
    }

    void ScreenRenderSetting::updateRendererConfiguration() {
        if (isRendererInitialized()) {
            // Update render buffers.
            auto optionalError = updateRenderBuffers();
            if (optionalError.has_value()) {
                auto error = optionalError.value();
                error.addEntry();
                error.showError();
                throw std::runtime_error(error.getError());
            }
        }
    }

    bool ScreenRenderSetting::isVsyncEnabled() const { return bIsVsyncEnabled; }

    void ScreenRenderSetting::setRefreshRate(std::pair<unsigned int, unsigned int> refreshRate) {
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
            getRenderSettingLogCategory());

        // Change.
        iRefreshRateNumerator = refreshRate.first;
        iRefreshRateDenominator = refreshRate.second;

        // TODO: need to change without restarting
        // updateRendererConfiguration();

        // Save.
        auto optionalError = saveConfigurationToDisk();
        if (optionalError.has_value()) {
            auto error = optionalError.value();
            error.addEntry();
            Logger::get().error(
                fmt::format(
                    "failed to save new render setting configuration, error: \"{}\"", error.getError()),
                getRenderSettingLogCategory());
        }
    }

    std::pair<unsigned int, unsigned int> ScreenRenderSetting::getRefreshRate() const {
        return std::make_pair(iRefreshRateNumerator, iRefreshRateDenominator);
    }

    void ScreenRenderSetting::setGpuToUse(const std::string& sGpuName) {
        if (sGpuName == getUsedGpuName()) {
            return; // do nothing
        }

        // Get names of supported GPUs.
        auto result = getRenderer()->getSupportedGpuNames();
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            Logger::get().error(
                fmt::format("failed to get the list of supported GPUs, error: \"{}\"", error.getError()),
                getRenderSettingLogCategory());
            return;
        }
        const auto vSupportedGpuNames = std::get<std::vector<std::string>>(std::move(result));

        // Make sure we fit into the `unsigned int` range.
        if (vSupportedGpuNames.size() > UINT_MAX) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "list of supported GPUs is too big to handle ({} items)", vSupportedGpuNames.size()),
                getRenderSettingLogCategory());
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
                    getRenderSettingLogCategory());

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
                            error.getError()),
                        getRenderSettingLogCategory());
                }
                return;
            }
        }

        // Log.
        Logger::get().error(
            fmt::format("failed to find the GPU \"{}\" in the list of supported GPUs", sGpuName),
            getRenderSettingLogCategory());
    }

    void ScreenRenderSetting::setGpuToUse(unsigned int iGpuIndex) {
        if (iGpuIndex == iUsedGpuIndex) {
            return; // do nothing
        }

        // Get names of supported GPUs.
        auto result = getRenderer()->getSupportedGpuNames();
        if (std::holds_alternative<Error>(result)) {
            auto error = std::get<Error>(std::move(result));
            error.addEntry();
            Logger::get().error(
                fmt::format("failed to get the list of supported GPUs, error: \"{}\"", error.getError()),
                getRenderSettingLogCategory());
            return;
        }
        const auto vSupportedGpuNames = std::get<std::vector<std::string>>(std::move(result));

        // Make sure we fit into the `unsigned int` range.
        if (vSupportedGpuNames.size() > UINT_MAX) [[unlikely]] {
            Logger::get().error(
                fmt::format(
                    "list of supported GPUs is too big to handle ({} items)", vSupportedGpuNames.size()),
                getRenderSettingLogCategory());
            return;
        }

        // Make sure this index is in range.
        if (iGpuIndex >= static_cast<unsigned int>(vSupportedGpuNames.size())) {
            Logger::get().error(
                fmt::format(
                    "specified GPU index to use ({}) is out of range, supported GPUs in total: {}",
                    iGpuIndex,
                    vSupportedGpuNames.size()),
                getRenderSettingLogCategory());
            return;
        }

        // Log change.
        Logger::get().info(
            fmt::format(
                "preferred GPU is being changed from \"{}\" to \"{}\"",
                vSupportedGpuNames[iUsedGpuIndex],
                vSupportedGpuNames[iGpuIndex]),
            getRenderSettingLogCategory());

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
                    "failed to save new render setting configuration, error: \"{}\"", error.getError()),
                getRenderSettingLogCategory());
        }
    }

    std::string ScreenRenderSetting::getUsedGpuName() const {
        return getRenderer()->getCurrentlyUsedGpuName();
    }

    unsigned int ScreenRenderSetting::getUsedGpuIndex() const { return iUsedGpuIndex; }
} // namespace ne
