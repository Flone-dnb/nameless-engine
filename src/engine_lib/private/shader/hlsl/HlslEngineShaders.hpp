﻿#pragma once

// Custom.
#include "shader/ShaderDescription.h"
#include "misc/ProjectPaths.h"
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
    };
} // namespace ne