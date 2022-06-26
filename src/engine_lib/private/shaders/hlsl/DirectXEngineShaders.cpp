#include "DirectXEngineShaders.h"

// External.
#include "xxHash/xxhash.h"

namespace ne {
    std::vector<std::string> shaderParametersToText(const std::set<DirectXShaderParameter>& vParams) {
        std::vector<std::string> vParameterNames;

        for (const auto& parameter : vParams) {
            switch (parameter) {
            case (DirectXShaderParameter::TEXTURE_FILTERING_POINT):
                vParameterNames.push_back("TEXTURE_FILTERING_POINT");
                break;
            case (DirectXShaderParameter::TEXTURE_FILTERING_LINEAR):
                vParameterNames.push_back("TEXTURE_FILTERING_LINEAR");
                break;
            case (DirectXShaderParameter::TEXTURE_FILTERING_ANISOTROPIC):
                vParameterNames.push_back("TEXTURE_FILTERING_ANISOTROPIC");
                break;
            case (DirectXShaderParameter::USE_DIFFUSE_TEXTURE):
                vParameterNames.push_back("USE_DIFFUSE_TEXTURE");
                break;
            case (DirectXShaderParameter::USE_NORMAL_TEXTURE):
                vParameterNames.push_back("USE_NORMAL_TEXTURE");
                break;
            }
        }

        return vParameterNames;
    }

    unsigned long long convertConfigurationToHash(const std::set<DirectXShaderParameter>& configuration) {
        if (configuration.empty())
            return 0;

        // Convert configuration numbers to string.
        std::string sConfiguration;
        for (const auto& parameter : configuration) {
            sConfiguration += std::to_string(static_cast<int>(parameter));
        }

        return XXH3_64bits(sConfiguration.c_str(), sConfiguration.size());
    }

    std::string convertConfigurationToText(const std::set<DirectXShaderParameter>& configuration) {
        if (configuration.empty())
            return "";

        // Calculate hash.
        return std::to_string(convertConfigurationToHash(configuration));
    }
} // namespace ne
