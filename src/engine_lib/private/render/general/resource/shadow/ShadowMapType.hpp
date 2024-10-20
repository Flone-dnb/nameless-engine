#pragma once

// Standard.
#include <stdexcept>
#include <string>

// Custom.
#include "misc/Error.h"

namespace ne {
    /** Determines type of a shadow map depending on the light source type. */
    enum class ShadowMapType : unsigned int {
        DIRECTIONAL = 0,
        SPOT,
        POINT,

        // ... new types go here ...

        SIZE // marks the size of this enum
    };

    /**
     * Converts shadow map type to string.
     *
     * @param type Type to convert.
     *
     * @return Name of the type.
     */
    inline std::string shadowMapTypeToString(ShadowMapType type) {
        switch (type) {
        case (ShadowMapType::DIRECTIONAL): {
            return "directional";
            break;
        }
        case (ShadowMapType::SPOT): {
            return "spot";
            break;
        }
        case (ShadowMapType::POINT): {
            return "point";
            break;
        }
        case (ShadowMapType::SIZE): {
            Error error("not a valid type");
            error.showError();
            throw std::runtime_error(error.getFullErrorMessage());
            break;
        }
        }

        Error error("unknown shadow map type");
        error.showError();
        throw std::runtime_error(error.getFullErrorMessage());
    }
}
