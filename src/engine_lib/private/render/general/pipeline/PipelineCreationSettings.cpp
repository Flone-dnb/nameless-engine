#include "PipelineCreationSettings.h"

namespace ne {

    ColorPipelineCreationSettings::ColorPipelineCreationSettings(
        const std::string& sPixelShaderName,
        std::set<ShaderMacro> additionalPixelShaderMacros,
        bool bUsePixelBlending)
        : additionalPixelShaderMacros(std::move(additionalPixelShaderMacros)),
          sPixelShaderName(sPixelShaderName), bUsePixelBlending(bUsePixelBlending) {}

    PipelineType ColorPipelineCreationSettings::getType() {
        if (bUsePixelBlending) {
            return PipelineType::PT_TRANSPARENT;
        }

        return PipelineType::PT_OPAQUE;
    }

    std::set<ShaderMacro> ColorPipelineCreationSettings::getAdditionalPixelShaderMacros() {
        return additionalPixelShaderMacros;
    }

    std::string ColorPipelineCreationSettings::getPixelShaderName() { return sPixelShaderName; }

    bool ColorPipelineCreationSettings::isPixelBlendingEnabled() { return bUsePixelBlending; }

    DepthPipelineCreationSettings::DepthPipelineCreationSettings(bool bUsedForShadowMapping)
        : bUsedForShadowMapping(bUsedForShadowMapping) {}

    PipelineType DepthPipelineCreationSettings::getType() {
        if (bUsedForShadowMapping) {
            return PipelineType::PT_SHADOW_MAPPING;
        }

        return PipelineType::PT_DEPTH_ONLY;
    }

    bool DepthPipelineCreationSettings::isDepthBiasEnabled() { return bUsedForShadowMapping; }

}
