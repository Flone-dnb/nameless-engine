#pragma once

// Standard.
#include <unordered_map>
#include <set>
#include <memory>
#include <string>
#include <array>

// Custom.
#include "shader/general/ShaderMacro.h"
#include "render/general/pipeline/PipelineType.hpp"

namespace ne {
    class Pipeline;

    /** Groups information about pipelines that use the same shaders. */
    struct ShaderPipelines {
        /**
         * Map of pairs "material defined macros" and
         * "pipelines that were created from the same shader to use these different macros".
         *          * @remark Since shader macros have prefixes that define which shader stage they are
         * valid for we won't have problems with same macros on different shader stages
         * (because of combining and storing all macros in just one `std::set`).
         */
        std::unordered_map<std::set<ShaderMacro>, std::shared_ptr<Pipeline>, ShaderMacroSetHash>
            shaderPipelines;
    };

    /** Stores pipelines of different types. */
    struct GraphicsPipelineRegistry {
        /** Map key is vertex (and pixel if specified) shader name(s). */
        std::array<
            std::unordered_map<std::string, ShaderPipelines>,
            static_cast<size_t>(GraphicsPipelineType::SIZE)>
            vPipelineTypes;
    };
}
