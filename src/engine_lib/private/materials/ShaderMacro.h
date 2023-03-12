#pragma once

// Standard.
#include <string>
#include <vector>
#include <set>
#include <unordered_map>

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
    std::vector<std::string> convertShaderMacrosToText(const std::set<ShaderMacro>& macros);

    /**
     * Formats an array of shader macros to a string in the form: "A, B, C".
     *
     * @param macros Macros to format into a string.
     *
     * @return Shader macros.
     */
    std::string formatShaderMacros(const std::vector<std::string>& macros);

    /** Defines valid shader macro combinations (configurations), plus some helper functions. */
    struct ShaderMacroConfigurations {
    private:
        /**
         * Combines the specified macro sets with macros to append.
         *
         * Example:
         * @code
         * // The following code:
         * combineConfigurations(
         *     {
         *         {NORMAL_TEXTURE}
         *     },
         *     {
         *         {DIFFUSE_TEXTURE},
         *         {FOO}
         *     },
         *     {
         *         TEXTURE_FILTERING_POINT,
         *         TEXTURE_FILTERING_LINEAR,
         *         TEXTURE_FILTERING_ANISOTROPIC
         *     },
         *     true);
         *
         * // Produces the following sets:
         * // 1. {} - empty set
         * // 2. {TEXTURE_FILTERING_POINT, DIFFUSE_TEXTURE}
         * // 3. {TEXTURE_FILTERING_POINT, FOO}
         * // 4. {TEXTURE_FILTERING_LINEAR, DIFFUSE_TEXTURE}
         * // 5. {TEXTURE_FILTERING_LINEAR, FOO}
         * // 5. {TEXTURE_FILTERING_ANISOTROPIC, DIFFUSE_TEXTURE}
         * // 6. {TEXTURE_FILTERING_ANISOTROPIC, FOO}
         * // 7. {NORMAL_TEXTURE}
         * @endcode
         *
         * @param constantSets               Sets that will be in the resulting set.
         * @param macroSets                  Sets to append macros to.
         * @param appendToEachSet            Each macro from this set will be added to each set of the
         * first argument.
         * @param bIncludeEmptyConfiguration Whether to add empty configuration to resulting sets or not.
         *
         * @return Resulting sets (modified macroSets argument).
         */
        static std::set<std::set<ShaderMacro>> combineConfigurations(
            const std::set<std::set<ShaderMacro>>& constantSets,
            const std::set<std::set<ShaderMacro>>& macroSets,
            const std::set<ShaderMacro>& appendToEachSet,
            bool bIncludeEmptyConfiguration);

        /**
         * Defines dependent macros that should be considered only when a specific macro is defined.
         * Stores pairs of "dependent macro" - "macro it depends on".
         */
        static inline const std::unordered_map<ShaderMacro, ShaderMacro> dependentMacros = {
            {ShaderMacro::TEXTURE_FILTERING_POINT, ShaderMacro::USE_DIFFUSE_TEXTURE},
            {ShaderMacro::TEXTURE_FILTERING_LINEAR, ShaderMacro::USE_DIFFUSE_TEXTURE},
            {ShaderMacro::TEXTURE_FILTERING_ANISOTROPIC, ShaderMacro::USE_DIFFUSE_TEXTURE},
        };

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

        /**
         * Tells if the specified macro is valid (should be considered) in the specified configuration.
         * Some macros depend on other which creates a situation where some macros should not be considered.
         *
         * @param macro         Macro to test.
         * @param configuration Configuration that the specified macro is going to be used.
         *
         * @return `true` if the specified macro should be used in the specified configuration, `false` if
         * some dependent macro does not exist in the specified configuration and thus makes the use of the
         * specified macro useless.
         */
        static bool isMacroShouldBeConsideredInConfiguration(
            ShaderMacro macro, const std::set<ShaderMacro>& configuration);

        /** Valid combinations of vertex shader macros. */
        static inline const std::set<std::set<ShaderMacro>> validVertexShaderMacroConfigurations = {{}};

        /** Valid combinations of pixel shader macros. */
        static inline const std::set<std::set<ShaderMacro>> validPixelShaderMacroConfigurations =
            combineConfigurations(
                {{ShaderMacro::USE_NORMAL_TEXTURE}},
                {{ShaderMacro::USE_DIFFUSE_TEXTURE}},
                {ShaderMacro::TEXTURE_FILTERING_POINT,
                 ShaderMacro::TEXTURE_FILTERING_LINEAR,
                 ShaderMacro::TEXTURE_FILTERING_ANISOTROPIC},
                true);

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
