#pragma once

// Standard.
#include <string>
#include <string_view>
#include <cstddef>

namespace ne {
    /** Used for `find` in `map` to work with `std::string_view` while the key is `std::string`. */
    struct StdStringHash {
        /** Tag. */
        using is_transparent = void;

        /**
         * Calculates hash.
         * @param pText Text to hash.
         * @return Hash.
         */
        std::size_t operator()(const char* pText) const { return std::hash<std::string_view>{}(pText); }

        /**
         * Calculates hash.
         * @param text Text to hash.
         * @return Hash.
         */
        std::size_t operator()(std::string_view text) const { return std::hash<std::string_view>{}(text); }

        /**
         * Calculates hash.
         * @param text Text to hash.
         * @return Hash.
         */
        std::size_t operator()(std::string const& text) const { return std::hash<std::string_view>{}(text); }
    };
}
