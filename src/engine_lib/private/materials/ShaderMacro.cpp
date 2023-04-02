﻿#include "ShaderMacro.h"

// External.
#include "xxHash/xxhash.h"

namespace ne {
    std::vector<std::string> convertShaderMacrosToText(const std::set<ShaderMacro>& macros) {
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
        const std::set<std::set<ShaderMacro>>& constantSets,
        const std::set<std::set<ShaderMacro>>& macroSets,
        const std::set<ShaderMacro>& appendToEachSet,
        bool bIncludeEmptyConfiguration) {
        // Define resulting set.
        std::set<std::set<ShaderMacro>> configurations;

        // Append macros.
        for (const auto& appendMacro : appendToEachSet) {
            for (const auto& set : macroSets) {
                auto setCopy = set;
                setCopy.insert(appendMacro);
                configurations.insert(std::move(setCopy));
            }
        }

        // Add empty configuration.
        if (bIncludeEmptyConfiguration) {
            configurations.insert(std::set<ShaderMacro>{});
        }

        // Append constant sets.
        for (const auto& set : constantSets) {
            configurations.insert(set);
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

    bool ShaderMacroConfigurations::isMacroShouldBeConsideredInConfiguration(
        ShaderMacro macro, const std::set<ShaderMacro>& configuration) {
        // See the specified macro in the list of dependent macros.
        auto dependentIt = dependentMacros.find(macro);
        if (dependentIt == dependentMacros.end()) {
            // Valid for this configuration because does not depend on other macros.
            return true;
        }

        // See if the specified configuration has a macro that the specified macro depends on.
        auto it = configuration.find(dependentIt->second);

        return it != configuration.end();
    }

    std::string formatShaderMacros(const std::vector<std::string>& macros) {
        if (macros.empty()) {
            return "";
        }

        std::string sOutput;

        for (const auto& macro : macros) {
            sOutput = macro + ", ";
        }

        sOutput.pop_back(); // pop last space
        sOutput.pop_back(); // pop last comma

        return sOutput;
    }

} // namespace ne