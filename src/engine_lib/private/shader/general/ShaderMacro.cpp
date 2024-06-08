#include "ShaderMacro.h"

// External.
#include "xxHash/xxhash.h"

namespace ne {
    std::vector<std::string> convertShaderMacrosToText(const std::set<ShaderMacro>& macros) {
        std::vector<std::string> vMacroNames;

        for (const auto& macro : macros) {
            switch (macro) {
            case (ShaderMacro::PS_USE_DIFFUSE_TEXTURE):
                vMacroNames.push_back("PS_USE_DIFFUSE_TEXTURE");
                break;
            case (ShaderMacro::VS_SHADOW_MAPPING_PASS):
                vMacroNames.push_back("VS_SHADOW_MAPPING_PASS");
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

    std::set<std::set<ShaderMacro>> ShaderMacroConfigurations::duplicateAndAppendConfiguration(
        const std::set<std::set<ShaderMacro>>& toDuplicateSets,
        const std::set<ShaderMacro>& toAppendToDuplicated) {
        std::set<std::set<ShaderMacro>> resultingSets;

        for (const auto& set : toDuplicateSets) {
            // Append the original set.
            resultingSets.insert(set);

            // Append the duplicated/modified set.
            auto duplicatedSet = set;
            for (const auto& macro : toAppendToDuplicated) {
                duplicatedSet.insert(macro);
            }
            resultingSets.insert(std::move(duplicatedSet));
        }

        return resultingSets;
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
