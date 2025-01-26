#pragma once

// Standard.
#include <variant>

// Custom.
#include "misc/Error.h"

namespace ne {
    class ConfigManager;

    /** Determines texture's filtering setting. */
    enum class TextureFilteringPreference : unsigned char {
        // DON'T change offsets in this enum, they match sampler offsets in the HLSL sampler heap
        POINT_FILTERING = 0,
        LINEAR_FILTERING = 1,
        ANISOTROPIC_FILTERING = 2,
        FROM_RENDER_SETTINGS, //< Determined according to the texture filtering quality from the render
                              // settings.
    };

    /**
     * Serializes the value to the specified config.
     *
     * @param config Config to add the value to.
     * @param value  Value to add.
     */
    void serializeTextureFilteringPreference(ConfigManager& config, TextureFilteringPreference value);

    /**
     * Deserializes the value from the specified config.
     *
     * @param config Config to read the value from.
     *
     * @return Error if the value is not found in the config or something went wrong, otherwise value from
     * the config.
     */
    std::variant<TextureFilteringPreference, Error>
    deserializeTextureFilteringPreference(ConfigManager& config);
}
