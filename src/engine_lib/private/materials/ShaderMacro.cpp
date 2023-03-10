#include "ShaderMacro.h"

// External.
#include "xxHash/xxhash.h"

namespace ne {
    std::vector<std::string> shaderMacrosToText(const std::set<ShaderMacro>& macros) {
        std::vector<std::string> vMacroNames;

        for (const auto& macro : macros) {
            switch (macro) {
            case (ShaderMacro::TEXTURE_FILTERING_POINT):
                vMacroNames.push_back("TEXTURE_FILTERING_POINT");
                break;
            case (ShaderMacro::TEXTURE_FILTERING_LINEAR):
                vMacroNames.push_back("TEXTURE_FILTERING_LINEAR");
                break;
            case (ShaderMacro::TEXTURE_FILTERING_ANISOTROPIC):
                vMacroNames.push_back("TEXTURE_FILTERING_ANISOTROPIC");
                break;
            case (ShaderMacro::USE_DIFFUSE_TEXTURE):
                vMacroNames.push_back("USE_DIFFUSE_TEXTURE");
                break;
            case (ShaderMacro::USE_NORMAL_TEXTURE):
                vMacroNames.push_back("USE_NORMAL_TEXTURE");
                break;
            }
        }

        return vMacroNames;
    }

    std::set<std::set<ShaderMacro>> ShaderMacroConfigurations::combineConfigurations(
        const std::set<ShaderMacro>& appendToEachSet,
        const std::set<std::set<ShaderMacro>>& macroSets,
        bool bIncludeEmptyConfiguration) {
        std::set<std::set<ShaderMacro>> configurations;
        if (bIncludeEmptyConfiguration) {
            configurations.insert(std::set<ShaderMacro>{});
        }

        for (const auto& appendMacro : appendToEachSet) {
            configurations.insert({appendMacro});

            for (const auto& set : macroSets) {
                auto setCopy = set;
                setCopy.insert(appendMacro);
                configurations.insert(setCopy);
            }
        }

        return configurations;
    }

    unsigned long long
    ShaderMacroConfigurations::convertConfigurationToHash(const std::set<ShaderMacro>& configuration) {
        if (configuration.empty()) {
            return 0;
        }

        // Convert configuration numbers to string.
        std::string sConfiguration;
        for (const auto& parameter : configuration) {
            sConfiguration += std::to_string(static_cast<int>(parameter));
        }

        return XXH3_64bits(sConfiguration.c_str(), sConfiguration.size());
    }

    std::string
    ShaderMacroConfigurations::convertConfigurationToText(const std::set<ShaderMacro>& configuration) {
        if (configuration.empty()) {
            return "";
        }

        // Calculate hash.
        return std::to_string(convertConfigurationToHash(configuration));
    }
} // namespace ne
