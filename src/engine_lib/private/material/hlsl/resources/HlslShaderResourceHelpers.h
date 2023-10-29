#pragma once

// Standard.
#include <variant>
#include <string>

// Custom.
#include "misc/Error.h"

namespace ne {
    class Pipeline;

    /** Provides static helper functions that HLSL shader resources use. */
    class HlslShaderResourceHelpers {
    public:
        HlslShaderResourceHelpers() = delete;

        /**
         * Looks for a root parameter that is used for the resource with the specified name.
         *
         * @param pPipeline           Pipeline to look for resource.
         * @param sShaderResourceName Resource name to look for.
         *
         * @return Error if something went wrong, otherwise root parameter index of the resource with
         * the specified name.
         */
        static std::variant<unsigned int, Error>
        getRootParameterIndexFromPipeline(Pipeline* pPipeline, const std::string& sShaderResourceName);
    };
}
