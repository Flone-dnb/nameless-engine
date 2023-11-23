#pragma once

// Custom.
#include "shader/ShaderDescription.h"
#include "misc/ProjectPaths.h"
#include "shader/general/EngineShaderConstantMacros.hpp"
#include "shader/general/EngineShaderNames.hpp"

namespace ne {
    /** Stores engine shader definitions used in Vulkan renderer. */
    class GlslEngineShaders {
    public:
        GlslEngineShaders() = delete;

        /** Mesh node's vertex shader. */
        static inline const auto meshNodeVertexShader = ShaderDescription(
            EngineShaderNames::MeshNode::sVertexShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/glsl/final/MeshNode.vert",
            ShaderType::VERTEX_SHADER,
            "main",
            {});

        /** Mesh node's fragment shader. */
        static inline const auto meshNodeFragmentShader = ShaderDescription(
            EngineShaderNames::MeshNode::sPixelShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/glsl/final/MeshNode.frag",
            ShaderType::PIXEL_SHADER,
            "main",
            {});

        /** Compute shader that calculate frustum for light tile that will be used in light culling. */
        static inline const auto forwardPlusCalculateGridFrustumComputeShader = ShaderDescription(
            EngineShaderNames::ForwardPlus::sCalculateFrustumGridComputeShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/glsl/final/light_culling/CalculateGridFrustums.comp",
            ShaderType::COMPUTE_SHADER,
            "main",
            {{
                EngineShaderConstantMacros::ForwardPlus::FrustumGridThreadsInGroupXyMacro::sName,
                EngineShaderConstantMacros::ForwardPlus::FrustumGridThreadsInGroupXyMacro::sValue,
            }});

        /** Compute shader that does light culling. */
        static inline const auto forwardPlusLightCullingComputeShader = ShaderDescription(
            EngineShaderNames::ForwardPlus::sLightCullingComputeShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/glsl/final/light_culling/LightCulling.comp",
            ShaderType::COMPUTE_SHADER,
            "main",
            {{
                 EngineShaderConstantMacros::ForwardPlus::FrustumGridThreadsInGroupXyMacro::sName,
                 EngineShaderConstantMacros::ForwardPlus::FrustumGridThreadsInGroupXyMacro::sValue,
             },
             {
                 EngineShaderConstantMacros::ForwardPlus::AveragePointLightNumPerTileMacro::sName,
                 EngineShaderConstantMacros::ForwardPlus::AveragePointLightNumPerTileMacro::sValue,
             },
             {
                 EngineShaderConstantMacros::ForwardPlus::AverageSpotLightNumPerTileMacro::sName,
                 EngineShaderConstantMacros::ForwardPlus::AverageSpotLightNumPerTileMacro::sValue,
             },
             {
                 EngineShaderConstantMacros::ForwardPlus::AverageDirectionalLightNumPerTileMacro::sName,
                 EngineShaderConstantMacros::ForwardPlus::AverageDirectionalLightNumPerTileMacro::sValue,
             }});

        /** Compute shader that resets global counts for light culling shader. */
        static inline const auto forwardPlusPrepareLightCullingComputeShader = ShaderDescription(
            EngineShaderNames::ForwardPlus::sPrepareLightCullingComputeShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/hlsl/light_culling/PrepareLightCulling.comp",
            ShaderType::COMPUTE_SHADER,
            "main",
            {});
    };
} // namespace ne
