#pragma once

// Custom.
#include "shader/ShaderDescription.h"
#include "misc/ProjectPaths.h"
#include "shader/general/EngineShaderConstantMacros.hpp"
#include "shader/general/EngineShaderNames.hpp"

namespace ne {
    /** Stores engine shader descriptions. */
    class EngineShaders {
    private:
        /**
         * Constructs a path to the shader source file.
         *
         * @param bIsHlsl                  `true` to construct a path to the HLSL file, `false` to GLSL.
         * @param sShaderPathRelativeFinal Path relative `final` shader directory to the shader without shader
         * language extension, for example: specify "MeshNode.vert" when the real file is
         * "MeshNode.vert.hlsl".
         *
         * @return Path to the shader source file.
         */
        static inline std::filesystem::path
        constructPathToShaderSourceFile(bool bIsHlsl, const std::string& sShaderPathRelativeFinal) {
            // Prepare language name string.
            const auto pLanguageName = bIsHlsl ? "hlsl" : "glsl";

            return ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) / "shaders" /
                   pLanguageName / "final" / (sShaderPathRelativeFinal + "." + pLanguageName);
        }

    public:
        EngineShaders() = delete;

        /** Groups MeshNode's shader descriptions. */
        struct MeshNode {
            /**
             * Returns MeshNode's vertex shader description.
             *
             * @param bIsHlsl `true` to construct a path to the HLSL file, `false` to GLSL.
             *
             * @return Shader description.
             */
            static inline ShaderDescription getVertexShader(bool bIsHlsl) {
                return ShaderDescription(
                    EngineShaderNames::MeshNode::getVertexShaderName(),
                    constructPathToShaderSourceFile(bIsHlsl, "MeshNode.vert"),
                    ShaderType::VERTEX_SHADER,
                    "main",
                    {});
            }

            /**
             * Returns MeshNode's fragment shader description.
             *
             * @param bIsHlsl `true` to construct a path to the HLSL file, `false` to GLSL.
             *
             * @return Shader description.
             */
            static inline ShaderDescription getFragmentShader(bool bIsHlsl) {
                return ShaderDescription(
                    EngineShaderNames::MeshNode::getFragmentShaderName(),
                    constructPathToShaderSourceFile(bIsHlsl, "MeshNode.frag"),
                    ShaderType::FRAGMENT_SHADER,
                    "main",
                    {EngineShaderConstantMacros::ForwardPlus::getLightGridTileSizeMacro()});
            }
        };

        /** Groups shaders used by point lights. */
        struct PointLight {
            /**
             * Returns fragment shader used in point light shadow passes.
             *
             * @param bIsHlsl `true` to construct a path to the HLSL file, `false` to GLSL.
             *
             * @return Shader description.
             */
            static inline ShaderDescription getFragmentShader(bool bIsHlsl) {
                return ShaderDescription(
                    EngineShaderNames::PointLight::getFragmentShaderName(),
                    constructPathToShaderSourceFile(bIsHlsl, "PointLight.frag"),
                    ShaderType::FRAGMENT_SHADER,
                    "main",
                    {EngineShaderConstantMacros::ForwardPlus::getLightGridTileSizeMacro(),
                     {"POINT_LIGHT_SHADOW_PASS", ""}});
            }
        };

        /** Groups shaders used in light culling process. */
        struct ForwardPlus {
            /**
             * Returns a compute shader description for a shader that calculates a frustum for a light grid
             * tile that will be used in light culling.
             *
             * @param bIsHlsl `true` to construct a path to the HLSL file, `false` to GLSL.
             *
             * @return Shader description.
             */
            static inline ShaderDescription getCalculateGridFrustumComputeShader(bool bIsHlsl) {
                return ShaderDescription(
                    EngineShaderNames::ForwardPlus::getCalculateFrustumGridComputeShaderName(),
                    constructPathToShaderSourceFile(bIsHlsl, "light_culling/CalculateGridFrustums.comp"),
                    ShaderType::COMPUTE_SHADER,
                    "main",
                    {EngineShaderConstantMacros::ForwardPlus::getLightGridTileSizeMacro()});
            }

            /**
             * Returns a compute shader description for a shader that resets global counts for light culling
             * shader.
             *
             * @param bIsHlsl `true` to construct a path to the HLSL file, `false` to GLSL.
             *
             * @return Shader description.
             */
            static inline ShaderDescription getPrepareLightCullingComputeShader(bool bIsHlsl) {
                return ShaderDescription(
                    EngineShaderNames::ForwardPlus::getPrepareLightCullingComputeShaderName(),
                    constructPathToShaderSourceFile(bIsHlsl, "light_culling/PrepareLightCulling.comp"),
                    ShaderType::COMPUTE_SHADER,
                    "main",
                    {});
            }

            /**
             * Returns a compute shader description for a shader that does light culling.
             *
             * @param bIsHlsl `true` to construct a path to the HLSL file, `false` to GLSL.
             *
             * @return Shader description.
             */
            static inline ShaderDescription getLightCullingComputeShader(bool bIsHlsl) {
                return ShaderDescription(
                    EngineShaderNames::ForwardPlus::getLightCullingComputeShaderName(),
                    constructPathToShaderSourceFile(bIsHlsl, "light_culling/LightCulling.comp"),
                    ShaderType::COMPUTE_SHADER,
                    "main",
                    {EngineShaderConstantMacros::ForwardPlus::getLightGridTileSizeMacro(),
                     EngineShaderConstantMacros::ForwardPlus::getAveragePointLightNumPerTileMacro(),
                     EngineShaderConstantMacros::ForwardPlus::getAverageSpotLightNumPerTileMacro()});
            }
        };
    };
} // namespace ne
