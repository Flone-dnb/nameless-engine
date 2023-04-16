#pragma once

// Standard.
#include <filesystem>
#include <string>

// Custom.
#include "math/GLMath.hpp"

namespace ne {
    /** Provides static helper functions. */
    class Globals {
        Globals() = delete;

    public:
        /** Groups vectors that point along world axes X, Y and Z. */
        struct WorldDirection {
            /** Vector that points in world's up direction. */
            static inline const glm::vec3 up = glm::vec3(0.0F, 0.0F, 1.0F); // NOLINT: short name

            /** Vector that points in world's right direction. */
            static inline const glm::vec3 right = glm::vec3(0.0F, 1.0F, 0.0F);

            /** Vector that points in world's forward direction. */
            static inline const glm::vec3 forward = glm::vec3(1.0F, 0.0F, 0.0F);
        };

        /**
         * Returns the name of this application.
         *
         * @return Name of this application.
         */
        static std::string getApplicationName();

        /**
         * Returns name of the directory used to store resources.
         *
         * @return Name of the directory.
         */
        static std::string getResourcesDirectoryName();

        /**
         * Returns name of the root (base) engine directory for storing configs, logs and etc.
         *
         * @return Name of the directory.
         */
        static std::string getEngineDirectoryName();

        /**
         * Converts wstring to its narrow multibyte representation.
         *
         * @param sText String to convert.
         *
         * @return Converted string.
         */
        static std::string wstringToString(const std::wstring& sText);

        /**
         * Converts string to wstring.
         *
         * @param sText String to convert.
         *
         * @return Converted string.
         */
        static std::wstring stringToWstring(const std::string& sText);

        /**
         * Returns name of the sub-category used for logs that exist only in debug builds.
         *
         * @return Name of the sub-category.
         */
        static std::string getDebugOnlyLoggingSubCategoryName();

    private:
        /** Name of the root (base) engine directory for storing configs, logs and etc. */
        static constexpr auto sBaseEngineDirectoryName = "nameless-engine";

        /** Name of the sub-category used for logs that exist only in debug builds. */
        static constexpr auto sDebugOnlyLoggingSubCategoryName = "Debug mode only";

        /** Name of the directory used to store resources. */
        static constexpr auto sResDirectoryName = "res";
    };
} // namespace ne
