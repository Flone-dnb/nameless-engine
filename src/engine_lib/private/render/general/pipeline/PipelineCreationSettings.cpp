#include "PipelineCreationSettings.h"

namespace ne {

    ColorPipelineCreationSettings::ColorPipelineCreationSettings(
        const std::string& sVertexShaderName,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::string& sPixelShaderName,
        std::set<ShaderMacro> additionalPixelShaderMacros)
        : PipelineCreationSettings(sVertexShaderName, additionalVertexShaderMacros),
          additionalPixelShaderMacros(std::move(additionalPixelShaderMacros)),
          sPixelShaderName(sPixelShaderName) {}

    PipelineType ColorPipelineCreationSettings::getType() { return PipelineType::PT_OPAQUE; }

    std::set<ShaderMacro> ColorPipelineCreationSettings::getAdditionalPixelShaderMacros() {
        return additionalPixelShaderMacros;
    }

    std::string ColorPipelineCreationSettings::getPixelShaderName() { return sPixelShaderName; }

    DepthPipelineCreationSettings::DepthPipelineCreationSettings(
        const std::string& sVertexShaderName,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        bool bUsedForShadowMapping)
        : PipelineCreationSettings(sVertexShaderName, additionalVertexShaderMacros),
          bUsedForShadowMapping(bUsedForShadowMapping) {
        // Add shadow mapping macro if enabled.
        if (bUsedForShadowMapping) {
            this->additionalVertexShaderMacros.insert(ShaderMacro::VS_SHADOW_MAPPING_PASS);
        }
    }

    PipelineType DepthPipelineCreationSettings::getType() {
        if (bUsedForShadowMapping) {
            return PipelineType::PT_SHADOW_MAPPING;
        }

        return PipelineType::PT_DEPTH_ONLY;
    }

    bool DepthPipelineCreationSettings::isDepthBiasEnabled() { return bUsedForShadowMapping; }

    PipelineCreationSettings::PipelineCreationSettings(
        const std::string& sVertexShaderName, const std::set<ShaderMacro>& additionalVertexShaderMacros)
        : additionalVertexShaderMacros(additionalVertexShaderMacros), sVertexShaderName(sVertexShaderName) {}

}
