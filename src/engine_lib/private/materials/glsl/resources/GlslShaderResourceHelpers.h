#pragma once

// Standard.
#include <variant>
#include <string>

// Custom.
#include "misc/Error.h"

namespace ne {
    class VulkanPipeline;

    /** Provides static helper functions that GLSL shader resources use. */
    class GlslShaderResourceHelpers {
    public:
        GlslShaderResourceHelpers() = delete;

        /**
         * Looks for an index that the specified shader resource should use to copy values to
         * push constants.
         *
         * @remark Generally used by shader resources that reference one or more items in a GLSL
         * array and the index into this array is defined in push constants (in GLSL). Shader
         * resources update a specific push constant to put some index (into some array) there.
         *
         * @param pVulkanPipeline     Pipeline to scan.
         * @param sShaderResourceName Name of the shader resource to look for.
         *
         * @return Error if something went wrong, otherwise binding index of the shader resource from
         * descriptor set layout.
         */
        static std::variant<size_t, Error>
        getPushConstantIndex(VulkanPipeline* pVulkanPipeline, const std::string& sShaderResourceName);
    };
}
