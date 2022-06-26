#pragma once

// STL.
#include <set>

// Custom.
#include "shaders/ShaderDescription.h"

namespace ne {
    enum class DirectXShaderParameter : int {
        TEXTURE_FILTERING_POINT = 0,
        TEXTURE_FILTERING_LINEAR,
        TEXTURE_FILTERING_ANISOTROPIC,
        USE_DIFFUSE_TEXTURE,
        // add new entries here...
        // also add new entries to @ref shaderParametersToText
    };

    /**
     * Converts shader parameters to array of text.
     *
     * @param vParams Shader parameters.
     *
     * @return Array of shader parameters in text form.
     */
    std::vector<std::string> shaderParametersToText(const std::set<DirectXShaderParameter>& vParams);

    /**
     * Converts configuration to hash.
     *
     * @param configuration Used shader configuration.
     *
     * @return Configuration in hash form.
     */
    unsigned long long convertConfigurationToHash(const std::set<DirectXShaderParameter>& configuration);

    /**
     * Converts configuration to text.
     *
     * @param configuration Used shader configuration.
     *
     * @return Configuration in text form. Usually should look something
     * like this: "1838281907459330133" (the hash of the specified configuration).
     */
    std::string convertConfigurationToText(const std::set<DirectXShaderParameter>& configuration);

    /** Provides hash operator() for std::set<DirectXShaderParameter>. */
    struct DirectXShaderParameterSetHash {
        /**
         * operator() that calculates hash from std::set<DirectXShaderParameter>,
         *
         * @param in Set of shader parameters.
         *
         * @return Hash.
         */
        size_t operator()(std::set<DirectXShaderParameter> const& in) const {
            return convertConfigurationToHash(in);
        }
    };

    /** Stores engine shader definitions used in DirectX renderer. */
    class DirectXEngineShaders {
    public:
        DirectXEngineShaders() = delete;
        DirectXEngineShaders(const DirectXEngineShaders&) = delete;
        DirectXEngineShaders& operator=(const DirectXEngineShaders&) = delete;

        /** Default vertex shader. */
        static inline const auto vertexShader = ShaderDescription(
            "engine.default.vs",
            "res/engine/shaders/default.hlsl",
            ShaderType::VERTEX_SHADER,
            "vsDefault",
            {});

        /** Default pixel shader. */
        static inline const auto pixelShader = ShaderDescription(
            "engine.default.ps",
            "res/engine/shaders/default.hlsl",
            ShaderType::PIXEL_SHADER,
            "psDefault",
            {});

        /** Valid combinations of vertex shader macros (settings). */
        static inline const std::set<std::set<DirectXShaderParameter>>
            validVertexShaderParameterCombinations = {{}};

        /** Valid combinations of pixel shader macros (settings). */
        static inline const std::set<std::set<DirectXShaderParameter>> validPixelShaderParameterCombinations =
            {{
                {DirectXShaderParameter::TEXTURE_FILTERING_POINT},
                {DirectXShaderParameter::TEXTURE_FILTERING_LINEAR},
                {DirectXShaderParameter::TEXTURE_FILTERING_ANISOTROPIC},
                {DirectXShaderParameter::TEXTURE_FILTERING_POINT,
                 DirectXShaderParameter::USE_DIFFUSE_TEXTURE},
                {DirectXShaderParameter::TEXTURE_FILTERING_LINEAR,
                 DirectXShaderParameter::USE_DIFFUSE_TEXTURE},
                {DirectXShaderParameter::TEXTURE_FILTERING_ANISOTROPIC,
                 DirectXShaderParameter::USE_DIFFUSE_TEXTURE},
            }};
    };
} // namespace ne
