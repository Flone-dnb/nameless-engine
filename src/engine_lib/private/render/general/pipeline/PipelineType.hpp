#pragma once

// Standard.
#include <cstddef>

namespace ne {
    /** Describes various pipeline types. */
    enum class PipelineType : size_t {
        PT_OPAQUE = 0,                      //< OPAQUE is a Windows macro, thus adding a prefix
        PT_TRANSPARENT,                     //< TRANSPARENT is a Windows macro, thus adding a prefix
        PT_DEPTH_ONLY,                      //< vertex shader only
        PT_SHADOW_MAPPING_DIRECTIONAL_SPOT, //< vertex shader only with depth bias
        PT_SHADOW_MAPPING_POINT,            //< vertex shader with depth bias and special fragment shader

        // !!! new Pipeline types go here !!!

        SIZE ///< marks the size of this enum, should be the last entry
    };

}
