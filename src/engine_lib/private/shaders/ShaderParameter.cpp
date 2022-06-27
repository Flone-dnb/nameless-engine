#include "ShaderParameter.h"

// External.
#include "xxHash/xxhash.h"

namespace ne {
    std::vector<std::string> shaderParametersToText(const std::set<ShaderParameter>& vParams) {
        std::vector<std::string> vParameterNames;

        for (const auto& parameter : vParams) {
            switch (parameter) {
            case (ShaderParameter::TEXTURE_FILTERING_POINT):
                vParameterNames.push_back("TEXTURE_FILTERING_POINT");
                break;
            case (ShaderParameter::TEXTURE_FILTERING_LINEAR):
                vParameterNames.push_back("TEXTURE_FILTERING_LINEAR");
                break;
            case (ShaderParameter::TEXTURE_FILTERING_ANISOTROPIC):
                vParameterNames.push_back("TEXTURE_FILTERING_ANISOTROPIC");
                break;
            case (ShaderParameter::USE_DIFFUSE_TEXTURE):
                vParameterNames.push_back("USE_DIFFUSE_TEXTURE");
                break;
            case (ShaderParameter::USE_NORMAL_TEXTURE):
                vParameterNames.push_back("USE_NORMAL_TEXTURE");
                break;
            }
        }

        return vParameterNames;
    }

    std::set<std::set<ShaderParameter>> ShaderParameterConfigurations::combineConfigurations(
        const std::set<ShaderParameter>& appendToEachSet,
        const std::set<std::set<ShaderParameter>>& parameterSets,
        bool bIncludeEmptyConfiguration) {
        std::set<std::set<ShaderParameter>> configurations;
        if (bIncludeEmptyConfiguration) {
            configurations.insert(std::set<ShaderParameter>{});
        }

        for (const auto& appendParam : appendToEachSet) {
            configurations.insert({appendParam});

            for (const auto& set : parameterSets) {
                auto setCopy = set;
                setCopy.insert(appendParam);
                configurations.insert(setCopy);
            }
        }

        return configurations;
    }

    unsigned long long ShaderParameterConfigurations::convertConfigurationToHash(
        const std::set<ShaderParameter>& configuration) {
        if (configuration.empty())
            return 0;

        // Convert configuration numbers to string.
        std::string sConfiguration;
        for (const auto& parameter : configuration) {
            sConfiguration += std::to_string(static_cast<int>(parameter));
        }

        return XXH3_64bits(sConfiguration.c_str(), sConfiguration.size());
    }

    std::string ShaderParameterConfigurations::convertConfigurationToText(
        const std::set<ShaderParameter>& configuration) {
        if (configuration.empty())
            return "";

        // Calculate hash.
        return std::to_string(convertConfigurationToHash(configuration));
    }
} // namespace ne
