#include "material/TextureFilteringPreference.h"

// Custom.
#include "io/ConfigManager.h"

namespace {
    static constexpr std::string_view sTextureFilteringPreferenceKeyName = "texture_filtering_preference";
    static constexpr std::string_view sSettingFromRenderSettings = "from render settings";
    static constexpr std::string_view sSettingAnisotropicFiltering = "anisotropic filtering";
    static constexpr std::string_view sSettingLinearFiltering = "linear filtering";
    static constexpr std::string_view sSettingPointFiltering = "point filtering";
}

namespace ne {

    void serializeTextureFilteringPreference(ConfigManager& config, TextureFilteringPreference value) {
        // Convert enum to string. Don't store enum's value because if the enum changes offsets can become
        // invalid.
        std::string_view sValueString = "";
        switch (value) {
        case (TextureFilteringPreference::FROM_RENDER_SETTINGS): {
            sValueString = sSettingFromRenderSettings;
            break;
        }
        case (TextureFilteringPreference::ANISOTROPIC_FILTERING): {
            sValueString = sSettingAnisotropicFiltering;
            break;
        }
        case (TextureFilteringPreference::LINEAR_FILTERING): {
            sValueString = sSettingLinearFiltering;
            break;
        }
        case (TextureFilteringPreference::POINT_FILTERING): {
            sValueString = sSettingPointFiltering;
            break;
        }
        default: {
            Error error("unhandled case");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
            break;
        }
        }

        config.setValue("", sTextureFilteringPreferenceKeyName, sValueString);
    }

    std::variant<TextureFilteringPreference, Error>
    deserializeTextureFilteringPreference(ConfigManager& config) {
        // Read value.
        auto optionalValue = config.getValueOrFail<std::string>("", sTextureFilteringPreferenceKeyName);
        if (!optionalValue.has_value()) [[unlikely]] {
            return Error(std::format(
                "key {} is not found or a conversion error occurred", sTextureFilteringPreferenceKeyName));
        }
        const auto sValueString = std::move(optionalValue.value());

        // Convert string to enum.
        if (sValueString == sSettingFromRenderSettings) {
            return TextureFilteringPreference::FROM_RENDER_SETTINGS;
        } else if (sValueString == sSettingPointFiltering) {
            return TextureFilteringPreference::POINT_FILTERING;
        } else if (sValueString == sSettingAnisotropicFiltering) {
            return TextureFilteringPreference::ANISOTROPIC_FILTERING;
        } else if (sValueString == sSettingLinearFiltering) {
            return TextureFilteringPreference::LINEAR_FILTERING;
        }

        return Error(
            std::format("key {} has unknown value \"{}\"", sTextureFilteringPreferenceKeyName, sValueString));
    }

}
