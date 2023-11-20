#pragma once

// Custom.
#include "shader/ShaderDescription.h"
#include "misc/ProjectPaths.h"
#include "shader/general/EngineShaderConstantMacros.hpp"
#include "shader/general/EngineShaderNames.hpp"

namespace ne {
    /** Stores engine shader definitions used in DirectX renderer. */
    class HlslEngineShaders {
    public:
        HlslEngineShaders() = delete;

        /** Mesh node's vertex shader. */
        static inline const auto meshNodeVertexShader = ShaderDescription(
            EngineShaderNames::MeshNode::sVertexShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/hlsl/MeshNode.vert.hlsl",
            ShaderType::VERTEX_SHADER,
            "vsMeshNode",
            {});

        /** Mesh node's pixel shader. */
        static inline const auto meshNodePixelShader = ShaderDescription(
            EngineShaderNames::MeshNode::sPixelShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/hlsl/MeshNode.frag.hlsl",
            ShaderType::PIXEL_SHADER,
            "psMeshNode",
            {});

        /** Compute shader that calculate frustum for light tile that will be used in light culling. */
        static inline const auto forwardPlusCalculateGridFrustumComputeShader = ShaderDescription(
            EngineShaderNames::ForwardPlus::sCalculateFrustumGridComputeShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/hlsl/light_culling/CalculateGridFrustums.comp.hlsl",
            ShaderType::COMPUTE_SHADER,
            "csGridFrustum",
            {{
                EngineShaderConstantMacros::ForwardPlus::FrustumGridThreadsInGroupXyMacro::sName,
                EngineShaderConstantMacros::ForwardPlus::FrustumGridThreadsInGroupXyMacro::sValue,
            }});

        /** Compute shader that does light culling. */
        static inline const auto forwardPlusLightCullingComputeShader = ShaderDescription(
            EngineShaderNames::ForwardPlus::sLightCullingComputeShaderName,
            ProjectPaths::getPathToResDirectory(ResourceDirectory::ENGINE) /
                "shaders/hlsl/light_culling/LightCulling.comp.hlsl",
            ShaderType::COMPUTE_SHADER,
            "csLightCulling",
            {{
                 EngineShaderConstantMacros::ForwardPlus::FrustumGridThreadsInGroupXyMacro::sName,
                 EngineShaderConstantMacros::ForwardPlus::FrustumGridThreadsInGroupXyMacro::sValue,
             },
             {
                 EngineShaderConstantMacros::ForwardPlus::AverageNumLightsOfSpecificTypePerTileMacro::sName,
                 EngineShaderConstantMacros::ForwardPlus::AverageNumLightsOfSpecificTypePerTileMacro::sValue,
             }});
    };
} // namespace ne
