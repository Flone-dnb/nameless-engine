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

    std::optional<GraphicsPipelineType> ColorPipelineConfiguration::getGraphicsType() const {
        if (bUsePixelBlending) {
            return GraphicsPipelineType::PT_TRANSPARENT;
        }

        return GraphicsPipelineType::PT_OPAQUE;
    }

    std::set<ShaderMacro> ColorPipelineConfiguration::getAdditionalPixelShaderMacros() const {
        return additionalPixelShaderMacros;
    }

    std::string_view ColorPipelineConfiguration::getPixelShaderName() const { return sPixelShaderName; }

    bool ColorPipelineConfiguration::isPixelBlendingEnabled() const { return bUsePixelBlending; }

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

    std::optional<GraphicsPipelineType> DepthPipelineConfiguration::getGraphicsType() const {
        if (shadowMappingUsage.has_value()) {
            if (shadowMappingUsage.value() == PipelineShadowMappingUsage::DIRECTIONAL_AND_SPOT_LIGHTS) {
                return GraphicsPipelineType::PT_SHADOW_MAPPING_DIRECTIONAL_SPOT;
            }

            return GraphicsPipelineType::PT_SHADOW_MAPPING_POINT;
        }

        return GraphicsPipelineType::PT_DEPTH_ONLY;
    }

    bool DepthPipelineConfiguration::isDepthBiasEnabled() const { return shadowMappingUsage.has_value(); }

    std::string_view DepthPipelineConfiguration::getPixelShaderName() const {
        if (shadowMappingUsage.has_value() &&
            shadowMappingUsage.value() == PipelineShadowMappingUsage::POINT_LIGHTS) {
            // Use a special fragment shader for shadow passes.
            return EngineShaderNames::PointLight::getFragmentShaderName();
        }

        return ""; // no pixel/fragment shader
    }

    std::optional<PipelineShadowMappingUsage> DepthPipelineConfiguration::getShadowMappingUsage() const {
        return shadowMappingUsage;
    }

    PipelineConfiguration::PipelineConfiguration(
        const std::string& sVertexShaderName, const std::set<ShaderMacro>& additionalVertexShaderMacros)
        : additionalVertexShaderMacros(additionalVertexShaderMacros), sVertexShaderName(sVertexShaderName) {}

    ComputePipelineConfiguration::ComputePipelineConfiguration(const std::string& sComputeShaderName)
        : PipelineConfiguration("", {}), sComputeShaderName(sComputeShaderName) {}

    std::optional<GraphicsPipelineType> ComputePipelineConfiguration::getGraphicsType() const { return {}; }

    std::string_view ComputePipelineConfiguration::getComputeShaderName() const { return sComputeShaderName; }

}
