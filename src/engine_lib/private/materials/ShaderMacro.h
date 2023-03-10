#pragma once

// Standard.
#include <string>
#include <vector>
#include <set>

namespace ne {
    /**
     * Defines macros that can be used in shaders,
     * macros will change based on the current render settings.
     *
     * A combination of shader macros is called a configuration,
     * one shader has different configurations.
     * A group of different shader configurations is stored in a shader pack.
     */
    enum class ShaderMacro : int {
        TEXTURE_FILTERING_POINT = 0,
        TEXTURE_FILTERING_LINEAR,
        TEXTURE_FILTERING_ANISOTROPIC,
        USE_DIFFUSE_TEXTURE,
        USE_NORMAL_TEXTURE,
        // add new entries here...
        // !! also add new entries to shaderMacrosToText !!
        // !! also add new entries to valid configuration combinations below !!
    };

    /**
     * Converts shader macros to array of text.
     *
     * @param macros Shader macros.
     *
     * @return Array of shader macros in text form.
     */
    std::vector<std::string> shaderMacrosToText(const std::set<ShaderMacro>& macros);

    /** Defines valid shader macro combinations (configurations), plus some helper functions. */
    struct ShaderMacroConfigurations {
    private:
        /**
         * Combines the specified macro sets with macros to append.
         *
         * @param appendToEachSet            Each macro from this set will be added to each set of the
         * second parameter.
         * @param macroSets                  Sets to append macros to.
         * @param bIncludeEmptyConfiguration Whether to add empty configuration to resulting sets or not.
         *
         * @return Resulting sets (modified macroSets argument).
         */
        static std::set<std::set<ShaderMacro>> combineConfigurations(
            const std::set<ShaderMacro>& appendToEachSet,
            const std::set<std::set<ShaderMacro>>& macroSets,
            bool bIncludeEmptyConfiguration);

    public:
        /**
         * Converts configuration to hash.
         *
         * @param configuration Used shader configuration.
         *
         * @return Configuration in hash form.
         */
        static unsigned long long convertConfigurationToHash(const std::set<ShaderMacro>& configuration);

        /**
         * Converts configuration to text.
         *
         * @param configuration Used shader configuration.
         *
         * @return Configuration in text form. Usually looks
         * like this: "1838281907459330133" (hash of the specified configuration).
         */
        static std::string convertConfigurationToText(const std::set<ShaderMacro>& configuration);

        /** Valid combinations of vertex shader macros. */
        static inline const std::set<std::set<ShaderMacro>> validVertexShaderMacroConfigurations = {{}};

        /** Valid combinations of pixel shader macros. */
        static inline const std::set<std::set<ShaderMacro>> validPixelShaderMacroConfigurations =
            combineConfigurations(
                {ShaderMacro::TEXTURE_FILTERING_POINT,
                 ShaderMacro::TEXTURE_FILTERING_LINEAR,
                 ShaderMacro::TEXTURE_FILTERING_ANISOTROPIC},
                {std::set<ShaderMacro>{},
                 {ShaderMacro::USE_DIFFUSE_TEXTURE},
                 {ShaderMacro::USE_DIFFUSE_TEXTURE, ShaderMacro::USE_NORMAL_TEXTURE}},
                false);

        /** Valid combinations of compute shader macros. */
        static inline const std::set<std::set<ShaderMacro>> validComputeShaderMacroConfigurations = {{}};
    };

    /** Provides hash operator() for std::set<ShaderMacro>. */
    struct ShaderMacroSetHash {
        /**
         * operator() that calculates hash from std::set<ShaderMacro>,
         *
         * @param item Set of shader macros.
         *
         * @return Hash.
         */
        size_t operator()(std::set<ShaderMacro> const& item) const {
            return ShaderMacroConfigurations::convertConfigurationToHash(item);
        }
    };
} // namespace ne
