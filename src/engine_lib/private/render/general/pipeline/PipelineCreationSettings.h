#pragma once

// Standard.
#include <set>

// Custom.
#include "render/general/pipeline/PipelineType.hpp"
#include "shader/general/ShaderMacro.h"

namespace ne {
    /**
     * Base class for pipeline creation options.
     *
     * @remark In order to specify pipeline creation settings create a new object of type that derives from
     * this class.
     */
    class PipelineCreationSettings {
    public:
        PipelineCreationSettings() = delete;
        virtual ~PipelineCreationSettings() = default;

        /**
         * Initializes options.
         *
         * @param sVertexShaderName            Name of the compiled vertex shader to use.
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
         */
        PipelineCreationSettings(
            const std::string& sVertexShaderName, const std::set<ShaderMacro>& additionalVertexShaderMacros);

        /**
         * Returns additional macros to enable for vertex shader configuration.
         *
         * @return Additional vertex shader macros to enable.
         */
        inline std::set<ShaderMacro> getAdditionalVertexShaderMacros() {
            return additionalVertexShaderMacros;
        }

        /**
         * Returns name of the vertex shader that should be used.
         *
         * @return Name of the compiled shader.
         */
        inline std::string getVertexShaderName() { return sVertexShaderName; }

        /**
         * Returns type of the pipeline that the object describes.
         *
         * @return Pipeline type.
         */
        virtual PipelineType getType() = 0;

        /**
         * Returns additional macros to enable for pixel/fragment shader configuration (if pixel/fragment
         * shader is used).
         *
         * @return Empty set if pixel/fragment shader is not used or no additional pixel/fragment shader
         * macros to enable.
         */
        virtual std::set<ShaderMacro> getAdditionalPixelShaderMacros() { return {}; }

        /**
         * Returns name of the pixel/fragment shader that should be used.
         *
         * @return Empty string of pixel/fragment shader is not used, otherwise name of the compiled shader.
         */
        virtual std::string getPixelShaderName() { return ""; }

        /**
         * Tells whether pixel blending should be enabled or not.
         *
         * @return `true` to enable, `false` to disable.
         */
        virtual bool isPixelBlendingEnabled() { return false; }

        /**
         * Tells whether depth bias (offset is enabled or not).
         *
         * @return `true` to enable, `false` to disable.
         */
        virtual bool isDepthBiasEnabled() { return false; }

    protected:
        /** Additional macros to enable for vertex shader configuration. */
        std::set<ShaderMacro> additionalVertexShaderMacros;

        /** Name of the compiled vertex shader to use. */
        const std::string sVertexShaderName;
    };

    /** Pipeline that uses pixel/fragment shaders to draw color. */
    class ColorPipelineCreationSettings : public PipelineCreationSettings {
    public:
        ColorPipelineCreationSettings() = delete;
        virtual ~ColorPipelineCreationSettings() override = default;

        /**
         * Initializes options.
         *
         * @param sVertexShaderName            Name of the compiled vertex shader to use.
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
         * @param sPixelShaderName             Name of the compiled pixel shader to use.
         * @param additionalPixelShaderMacros  Additional macros to enable for pixel shader configuration.
         * @param bUsePixelBlending            `true` to enable transparency, `false` to disable.
         */
        ColorPipelineCreationSettings(
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
        virtual PipelineType getType() override;

        /**
         * Returns additional macros to enable for pixel/fragment shader configuration (if pixel/fragment
         * shader is used).
         *
         * @return Macros.
         */
        virtual std::set<ShaderMacro> getAdditionalPixelShaderMacros() override;

        /**
         * Returns name of the pixel/fragment shader that should be used.
         *
         * @return Empty string of pixel/fragment shader is not used, otherwise name of the compiled shader.
         */
        virtual std::string getPixelShaderName() override;

        /**
         * Tells whether pixel blending should be enabled or not.
         *
         * @return `true` to enable, `false` to disable.
         */
        virtual bool isPixelBlendingEnabled() override;

    protected:
        /** Additional macros to enable for pixel shader configuration. */
        std::set<ShaderMacro> additionalPixelShaderMacros;

        /** Name of the compiled pixel shader to use. */
        const std::string sPixelShaderName;

        /** `true` to enable transparency, `false` to disable. */
        const bool bUsePixelBlending;
    };

    /** Pipeline that only uses vertex shader to draw depth. */
    class DepthPipelineCreationSettings : public PipelineCreationSettings {
    public:
        DepthPipelineCreationSettings() = delete;
        virtual ~DepthPipelineCreationSettings() override = default;

        /**
         * Initializes options.
         *
         * @param sVertexShaderName            Name of the compiled vertex shader to use.
         * @param additionalVertexShaderMacros Additional macros to enable for vertex shader configuration.
         * @param bUsedForShadowMapping        Specify `true` if the pipeline will be used for shadow mapping
         * and thus shadow bias should be used, otherwise `false`.
         */
        DepthPipelineCreationSettings(
            const std::string& sVertexShaderName,
            const std::set<ShaderMacro>& additionalVertexShaderMacros,
            bool bUsedForShadowMapping);

        /**
         * Returns type of the pipeline that the object describes.
         *
         * @return Pipeline type.
         */
        virtual PipelineType getType() override;

        /**
         * Tells whether depth bias (offset is enabled or not).
         *
         * @return `true` to enable, `false` to disable.
         */
        virtual bool isDepthBiasEnabled() override;

    protected:
        /** `true` to enable shadow bias, `false` otherwise. */
        const bool bUsedForShadowMapping;
    };
}
