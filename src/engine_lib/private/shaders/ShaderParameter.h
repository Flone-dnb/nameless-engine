#pragma once

// Standard.
#include <string>
#include <vector>
#include <set>

namespace ne {
    /**
     * Defines macros that can be used in shaders,
     * macros will change based on the current settings.
     *
     * A combination of shader parameters is called a configuration,
     * one shader will have different variants for different configurations.
     * A group of different shader variants is stored in a shader pack.
     */
    enum class ShaderParameter : int {
        TEXTURE_FILTERING_POINT = 0,
        TEXTURE_FILTERING_LINEAR,
        TEXTURE_FILTERING_ANISOTROPIC,
        USE_DIFFUSE_TEXTURE,
        USE_NORMAL_TEXTURE,
        // add new entries here...
        // !! also add new entries to @ref shaderParametersToText !!
        // !! also add new entries to valid configuration combinations below !!
    };

    /**
     * Converts shader parameters to array of text.
     *
     * @param params Shader parameters.
     *
     * @return Array of shader parameters in text form.
     */
    std::vector<std::string> shaderParametersToText(const std::set<ShaderParameter>& params);

    /**
     * Defines valid shader parameter combinations (configurations), plus some helper functions.
     */
    struct ShaderParameterConfigurations {
    private:
        /**
         * Combines the specified parameter sets with parameters to append.
         *
         * @param appendToEachSet Each parameter from this set will be added to each set of the
         * second parameter.
         * @param parameterSets   Sets to append parameters to.
         * @param bIncludeEmptyConfiguration Whether to add empty configuration to resulting sets.
         *
         * @return Resulting sets (modified parameterSets argument).
         */
        static std::set<std::set<ShaderParameter>> combineConfigurations(
            const std::set<ShaderParameter>& appendToEachSet,
            const std::set<std::set<ShaderParameter>>& parameterSets,
            bool bIncludeEmptyConfiguration);

    public:
        /**
         * Converts configuration to hash.
         *
         * @param configuration Used shader configuration.
         *
         * @return Configuration in hash form.
         */
        static unsigned long long convertConfigurationToHash(const std::set<ShaderParameter>& configuration);

        /**
         * Converts configuration to text.
         *
         * @param configuration Used shader configuration.
         *
         * @return Configuration in text form. Usually should look something
         * like this: "1838281907459330133" (the hash of the specified configuration).
         */
        static std::string convertConfigurationToText(const std::set<ShaderParameter>& configuration);

        /** Valid combinations of vertex shader macros (settings). */
        static inline const std::set<std::set<ShaderParameter>> validVertexShaderParameterConfigurations = {
            {}};

        /** Valid combinations of pixel shader macros (settings). */
        static inline const std::set<std::set<ShaderParameter>> validPixelShaderParameterConfigurations =
            combineConfigurations(
                {ShaderParameter::TEXTURE_FILTERING_POINT,
                 ShaderParameter::TEXTURE_FILTERING_LINEAR,
                 ShaderParameter::TEXTURE_FILTERING_ANISOTROPIC},
                {std::set<ShaderParameter>{},
                 {ShaderParameter::USE_DIFFUSE_TEXTURE},
                 {ShaderParameter::USE_DIFFUSE_TEXTURE, ShaderParameter::USE_NORMAL_TEXTURE}},
                false);

        /** Valid combinations of compute shader macros (settings). */
        static inline const std::set<std::set<ShaderParameter>> validComputeShaderParameterConfigurations = {
            {}};
    };

    /** Provides hash operator() for std::set<ShaderParameter>. */
    struct ShaderParameterSetHash {
        /**
         * operator() that calculates hash from std::set<ShaderParameter>,
         *
         * @param item Set of shader parameters.
         *
         * @return Hash.
         */
        size_t operator()(std::set<ShaderParameter> const& item) const {
            return ShaderParameterConfigurations::convertConfigurationToHash(item);
        }
    };
} // namespace ne
