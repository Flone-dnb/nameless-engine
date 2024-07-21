#include "PipelineConfiguration.h"

// Custom.
#include "shader/general/EngineShaderNames.hpp"

namespace ne {

    ColorPipelineConfiguration::ColorPipelineConfiguration(
        const std::string& sVertexShaderName,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        const std::string& sPixelShaderName,
        std::set<ShaderMacro> additionalPixelShaderMacros,
        bool bUsePixelBlending)
        : PipelineConfiguration(sVertexShaderName, additionalVertexShaderMacros),
          additionalPixelShaderMacros(std::move(additionalPixelShaderMacros)),
          sPixelShaderName(sPixelShaderName), bUsePixelBlending(bUsePixelBlending) {}

    PipelineType ColorPipelineConfiguration::getType() {
        if (bUsePixelBlending) {
            return PipelineType::PT_TRANSPARENT;
        }

        return PipelineType::PT_OPAQUE;
    }

    std::set<ShaderMacro> ColorPipelineConfiguration::getAdditionalPixelShaderMacros() {
        return additionalPixelShaderMacros;
    }

    std::string ColorPipelineConfiguration::getPixelShaderName() { return sPixelShaderName; }

    bool ColorPipelineConfiguration::isPixelBlendingEnabled() { return bUsePixelBlending; }

    DepthPipelineConfiguration::DepthPipelineConfiguration(
        const std::string& sVertexShaderName,
        const std::set<ShaderMacro>& additionalVertexShaderMacros,
        std::optional<PipelineShadowMappingUsage> shadowMappingUsage)
        : PipelineConfiguration(sVertexShaderName, additionalVertexShaderMacros),
          shadowMappingUsage(shadowMappingUsage) {
        // Add shadow mapping macro if enabled.
        if (shadowMappingUsage.has_value()) {
            this->additionalVertexShaderMacros.insert(ShaderMacro::VS_SHADOW_MAPPING_PASS);
        }
    }

    PipelineType DepthPipelineConfiguration::getType() {
        if (shadowMappingUsage.has_value()) {
            if (shadowMappingUsage.value() == PipelineShadowMappingUsage::DIRECTIONAL_AND_SPOT_LIGHTS) {
                return PipelineType::PT_SHADOW_MAPPING_DIRECTIONAL_SPOT;
            }

            return PipelineType::PT_SHADOW_MAPPING_POINT;
        }

        return PipelineType::PT_DEPTH_ONLY;
    }

    bool DepthPipelineConfiguration::isDepthBiasEnabled() { return shadowMappingUsage.has_value(); }

    std::string DepthPipelineConfiguration::getPixelShaderName() {
        if (shadowMappingUsage.has_value() &&
            shadowMappingUsage.value() == PipelineShadowMappingUsage::POINT_LIGHTS) {
            // Usage a special fragment shader for shadow passes.
            return EngineShaderNames::PointLight::getFragmentShaderName();
        }

        return ""; // no pixel/fragment shader
    }

    std::optional<PipelineShadowMappingUsage> DepthPipelineConfiguration::getShadowMappingUsage() {
        return shadowMappingUsage;
    }

    PipelineConfiguration::PipelineConfiguration(
        const std::string& sVertexShaderName, const std::set<ShaderMacro>& additionalVertexShaderMacros)
        : additionalVertexShaderMacros(additionalVertexShaderMacros), sVertexShaderName(sVertexShaderName) {}

}
