#pragma once

// Standard.
#include <set>
#include <optional>

// Custom.
#include "render/general/pipeline/PipelineType.hpp"
#include "shader/general/ShaderMacro.h"

namespace ne {
    /** Defines which light sources will be used with the pipeline in shadow mapping. */
    enum class PipelineShadowMappingUsage {
        DIRECTIONAL_AND_SPOT_LIGHTS,
        POINT_LIGHTS,
    };

    /**
     * Base class for pipeline options.
     *
     * @remark In order to specify pipeline settings create a new object of a type that derives from
     * this class.
     */
    class PipelineConfiguration {
    public:
        PipelineConfiguration() = delete;
        virtual ~PipelineConfiguration() = default;

        /**
         * Returns additional macros to enable for vertex shader configuration.
         *
         * @return Additional vertex shader macros to enable.
         */
        inline std::set<ShaderMacro> getAdditionalVertexShaderMacros() const {
            return additionalVertexShaderMacros;
        }

        /**
         * Returns name of the vertex shader that should be used.
         *
         * @return Name of the compiled shader.
         */
        inline std::string_view getVertexShaderName() const { return sVertexShaderName; }

        /**
         * Returns type of the pipeline that the object describes.
         *
         * @return Empty if not a graphics pipeline, otherwise graphics pipeline type.
         */
        virtual std::optional<GraphicsPipelineType> getGraphicsType() const = 0;

        /**
         * Returns additional macros to enable for pixel/fragment shader configuration (if pixel/fragment
         * shader is used).
         *
         * @return Empty set if pixel/fragment shader is not used or no additional pixel/fragment shader
         * macros to enable.
         */
        virtual std::set<ShaderMacro> getAdditionalPixelShaderMacros() const { return {}; }

        /**
         * Returns name of the pixel/fragment shader that should be used.
         *
         * @return Empty string if pixel/fragment shader is not used, otherwise name of the compiled shader.
         */
        virtual std::string_view getPixelShaderName() const { return ""; }

        /**
         * Returns name of the compute shader that should be used.
         *
         * @return Empty string if compute shader is not used, otherwise name of the compiled shader.
         */
        virtual std::string_view getComputeShaderName() const { return ""; }

        /**
         * Tells whether pixel blending should be enabled or not.
         *
         * @return `true` to enable, `false` to disable.
         */
        virtual bool isPixelBlendingEnabled() const { return false; }

        /**
         * Tells whether depth bias (offset is enabled or not).
         *
         * @return `true` to enable, `false` to disable.
         */
        virtual bool isDepthBiasEnabled() const { return false; }

        /**
         * Tells if this pipeline is used in shadow mapping.
         *
         * @return Empty if not used in shadow mapping, otherwise light sources that can use the pipeline
         * for shadow mapping.
         */
        virtual std::optional<PipelineShadowMappingUsage> getShadowMappingUsage() const { return {}; }

    protected:
        /**
         * Initializes options.
         *
         * @param sVertexShaderName            Name of the compiled vertex shader to use (empty if not used).
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
         */
        PipelineConfiguration(
            const std::string& sVertexShaderName, const std::set<ShaderMacro>& additionalVertexShaderMacros);

        /** Additional macros to enable for vertex shader configuration. */
        std::set<ShaderMacro> additionalVertexShaderMacros;

        /** Name of the compiled vertex shader to use. */
        const std::string sVertexShaderName;
    };

    /** Pipeline that uses pixel/fragment shaders to draw color. */
    class ColorPipelineConfiguration : public PipelineConfiguration {
    public:
        ColorPipelineConfiguration() = delete;
        virtual ~ColorPipelineConfiguration() override = default;

        /**
         * Initializes options.
         *
         * @param sVertexShaderName            Name of the compiled vertex shader to use.
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
         * @param sPixelShaderName             Name of the compiled pixel shader to use.
         * @param additionalPixelShaderMacros  Additional macros to enable for pixel shader configuration.
         * @param bUsePixelBlending            `true` to enable transparency, `false` to disable.
         */
        ColorPipelineConfiguration(
            const std::string& sVertexShaderName,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            const std::string& sPixelShaderName,
            std::set<ShaderMacro> additionalPixelShaderMacros,
            bool bUsePixelBlending);

        /**
         * Returns type of the pipeline that the object describes.
         *
         * @return Pipeline type.
         */
        virtual std::optional<GraphicsPipelineType> getGraphicsType() const override;

        /**
         * Returns additional macros to enable for pixel/fragment shader configuration (if pixel/fragment
         * shader is used).
         *
         * @return Macros.
         */
        virtual std::set<ShaderMacro> getAdditionalPixelShaderMacros() const override;

        /**
         * Returns name of the pixel/fragment shader that should be used.
         *
         * @return Empty string if pixel/fragment shader is not used, otherwise name of the compiled shader.
         */
        virtual std::string_view getPixelShaderName() const override;

        /**
         * Tells whether pixel blending should be enabled or not.
         *
         * @return `true` to enable, `false` to disable.
         */
        virtual bool isPixelBlendingEnabled() const override;

    protected:
        /** Additional macros to enable for pixel shader configuration. */
        std::set<ShaderMacro> additionalPixelShaderMacros;

        /** Name of the compiled pixel shader to use. */
        const std::string sPixelShaderName;

        /** `true` to enable transparency, `false` to disable. */
        const bool bUsePixelBlending;
    };

    /** Pipeline that only uses vertex shader to draw depth. */
    class DepthPipelineConfiguration : public PipelineConfiguration {
    public:
        DepthPipelineConfiguration() = delete;
        virtual ~DepthPipelineConfiguration() override = default;

        /**
         * Initializes options.
         *
         * @param sVertexShaderName            Name of the compiled vertex shader to use.
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
         * @param shadowMappingUsage           Specify empty if this pipeline is not used in the shadow
         * mapping, otherwise specify which light sources will be able to use this pipeline in shadow mapping
         * and thus shadow bias should be used.
         */
        DepthPipelineConfiguration(
            const std::string& sVertexShaderName,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            std::optional<PipelineShadowMappingUsage> shadowMappingUsage);

        /**
         * Returns type of the pipeline that the object describes.
         *
         * @return Pipeline type.
         */
        virtual std::optional<GraphicsPipelineType> getGraphicsType() const override;

        /**
         * Tells whether depth bias (offset is enabled or not).
         *
         * @return `true` to enable, `false` to disable.
         */
        virtual bool isDepthBiasEnabled() const override;

        /**
         * Returns name of the pixel/fragment shader that should be used.
         *
         * @return Empty string if pixel/fragment shader is not used, otherwise name of the compiled shader.
         */
        virtual std::string_view getPixelShaderName() const override;

        /**
         * Tells if this pipeline is used in shadow mapping.
         *
         * @return Empty if not used in shadow mapping, otherwise light sources that can use the pipeline
         * for shadow mapping.
         */
        virtual std::optional<PipelineShadowMappingUsage> getShadowMappingUsage() const override;

    protected:
        /** Empty if not used for shadow mapping, otherwise light sources that can use it. */
        const std::optional<PipelineShadowMappingUsage> shadowMappingUsage;
    };

    /** Pipeline that uses a compute shader. */
    class ComputePipelineConfiguration : public PipelineConfiguration {
    public:
        ComputePipelineConfiguration() = delete;
        virtual ~ComputePipelineConfiguration() override = default;

        /**
         * Initializes options.
         *
         * @param sComputeShaderName Name of the compute shader to use.
         */
        ComputePipelineConfiguration(const std::string& sComputeShaderName);

        /**
         * Returns type of the pipeline that the object describes.
         *
         * @return Pipeline type.
         */
        virtual std::optional<GraphicsPipelineType> getGraphicsType() const override;

        /**
         * Returns name of the compute shader that should be used.
         *
         * @return Empty string if compute shader is not used, otherwise name of the compiled shader.
         */
        virtual std::string_view getComputeShaderName() const override;

    private:
        /** Name of a compiled compute shader to use. */
        const std::string sComputeShaderName;
    };
}
