#pragma once

// External.
#include "directx/d3dx12.h"

namespace ne {
    /**
     * Returns name of the shader resource that stores all texture samplers.
     *
     * @return Shader resource name.
     */
    static constexpr std::string_view getTextureSamplersShaderArrayName() { return "textureSamplers"; }

    /**
     * Returns point sampler description.
     *
     * @return Sampler description.
     */
    static D3D12_SAMPLER_DESC getPointSamplerDescription() {
        return D3D12_SAMPLER_DESC(
            D3D12_FILTER_MIN_MAG_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            0.0F,
            16, // NOLINT: magic number, max anisotropy
            D3D12_COMPARISON_FUNC_NEVER,
            {1.0F, 1.0F, 1.0F, 1.0F}, // border color if border address mode
            0.0F,
            D3D12_FLOAT32_MAX);
    }

    /**
     * Returns linear sampler description.
     *
     * @return Sampler description.
     */
    static D3D12_SAMPLER_DESC getLinearSamplerDescription() {
        return D3D12_SAMPLER_DESC(
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            0.0F,
            16, // NOLINT: magic number, max anisotropy
            D3D12_COMPARISON_FUNC_NEVER,
            {1.0F, 1.0F, 1.0F, 1.0F}, // border color if border address mode
            0.0F,
            D3D12_FLOAT32_MAX);
    }

    /**
     * Returns anisotropic sampler description.
     *
     * @return Sampler description.
     */
    static D3D12_SAMPLER_DESC getAnisotropicSamplerDescription() {
        return D3D12_SAMPLER_DESC(
            D3D12_FILTER_ANISOTROPIC,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,
            0.0F,
            16, // NOLINT: magic number, max anisotropy
            D3D12_COMPARISON_FUNC_NEVER,
            {1.0F, 1.0F, 1.0F, 1.0F}, // border color if border address mode
            0.0F,
            D3D12_FLOAT32_MAX);
    }

    /**
     * Returns description of a static comparison sampler.
     *
     * @return Static sampler description.
     */
    static CD3DX12_STATIC_SAMPLER_DESC getStaticComparisonSamplerDescription() {
        return CD3DX12_STATIC_SAMPLER_DESC(
            0,
            D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,
            0.0F,
            16,                               // NOLINT: magic number, max anisotropy
            D3D12_COMPARISON_FUNC_LESS_EQUAL, // same as in Vulkan
            D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK,
            0.0F,
            D3D12_FLOAT32_MAX,
            D3D12_SHADER_VISIBILITY_ALL,
            6);
    }
}
