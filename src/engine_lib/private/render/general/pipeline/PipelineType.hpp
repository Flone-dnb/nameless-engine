#pragma once

namespace ne {
    /** Describes various pipeline types. */
    enum class PipelineType : size_t {
        PT_OPAQUE = 0,     //< OPAQUE is a Windows macro, thus adding a prefix
        PT_DEPTH_ONLY,     //< vertex shader only
        PT_SHADOW_MAPPING, //< vertex shader only with depth bias

        // !!! new Pipeline types go here !!!

        SIZE ///< marks the size of this enum, should be the last entry
    };

}
