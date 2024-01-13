#include "PipelineCreationSettings.h"

// Custom.
#include "shader/general/EngineShaderNames.hpp"

namespace ne {

    ColorPipelineCreationSettings::ColorPipelineCreationSettings(
        const std::string& sVertexShaderName,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::string& sPixelShaderName,
        std::set<ShaderMacro> additionalPixelShaderMacros,
        bool bUsePixelBlending)
        : PipelineCreationSettings(sVertexShaderName, additionalVertexShaderMacros),
          additionalPixelShaderMacros(std::move(additionalPixelShaderMacros)),
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

    DepthPipelineCreationSettings::DepthPipelineCreationSettings(
        const std::string& sVertexShaderName,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        std::optional<PipelineShadowMappingUsage> shadowMappingUsage)
        : PipelineCreationSettings(sVertexShaderName, additionalVertexShaderMacros),
          shadowMappingUsage(shadowMappingUsage) {
        // Add shadow mapping macro if enabled.
        if (shadowMappingUsage.has_value()) {
            this->additionalVertexShaderMacros.insert(ShaderMacro::VS_SHADOW_MAPPING_PASS);
        }
    }

    PipelineType DepthPipelineCreationSettings::getType() {
        if (shadowMappingUsage.has_value()) {
            if (shadowMappingUsage.value() == PipelineShadowMappingUsage::DIRECTIONAL_AND_SPOT_LIGHTS) {
                return PipelineType::PT_SHADOW_MAPPING_DIRECTIONAL_SPOT;
            }

            return PipelineType::PT_SHADOW_MAPPING_POINT;
        }

        return PipelineType::PT_DEPTH_ONLY;
    }

    bool DepthPipelineCreationSettings::isDepthBiasEnabled() { return shadowMappingUsage.has_value(); }

    std::string DepthPipelineCreationSettings::getPixelShaderName() {
        if (shadowMappingUsage.has_value() &&
            shadowMappingUsage.value() == PipelineShadowMappingUsage::POINT_LIGHTS) {
            // Usage a special fragment shader for shadow passes.
            return EngineShaderNames::PointLight::getFragmentShaderName();
        }

        return ""; // no pixel/fragment shader
    }

    std::optional<PipelineShadowMappingUsage> DepthPipelineCreationSettings::getShadowMappingUsage() {
        return shadowMappingUsage;
    }

    PipelineCreationSettings::PipelineCreationSettings(
        const std::string& sVertexShaderName, const std::set<ShaderMacro>& additionalVertexShaderMacros)
        : additionalVertexShaderMacros(additionalVertexShaderMacros), sVertexShaderName(sVertexShaderName) {}

}
