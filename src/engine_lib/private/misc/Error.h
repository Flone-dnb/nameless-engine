#pragma once

// STL.
#include <string_view>
#include <vector>
#include <string>
#include "source_location.hpp" // TODO: remove when Clang or GCC will have support for std::source_location

#if defined(WIN32)
#include <Windows.h>
#endif

namespace ne {
    /** Information of a specific source code location. */
    struct SourceLocationInfo {
        /** Filename. */
        std::string sFilename;
        /** Line number. */
        std::string sLine;
    };

    /**
     * Helper class to store error messages.
     */
    class Error {
    public:
        /**
         * Construct a new Error object.
         *
         * @param sMessage  Error message to show.
         * @param location  Should be empty.
         */
        Error(
            std::string_view sMessage,
            const nostd::source_location location = nostd::source_location::current());

#if defined(WIN32)
        /**
         * Construct a new Error object from HRESULT.
         *
         * @param hResult   HRESULT that contains an error.
         * @param location  Should be empty.
         */
        Error(
            const HRESULT hResult, const nostd::source_location location = nostd::source_location::current());

        /**
         * Construct a new Error object.
         *
         * @param errorCode Error code returned by GetLastError().
         * @param location  Should be empty.
         */
        Error(
            unsigned long errorCode,
            const nostd::source_location location = nostd::source_location::current());
#endif

        Error() = delete;
        virtual ~Error() = default;

        /**
         * Copy constructor.
         *
         * @param other other object.
         */
        Error(const Error& other) = default;

        /**
         * Copy assignment.
         *
         * @param other other object.
         *
         * @return Result of copy assignment.
         */
        Error& operator=(const Error& other) = default;

        /**
         * Move constructor.
         *
         * @param other other object.
         */
        Error(Error&& other) = default;

        /**
         * Move assignment.
         *
         * @param other other object.
         *
         * @return Result of move assignment.
         */
        Error& operator=(Error&& other) = default;

        /**
         * Adds an entry to the error stack.
         *
         * @param location  Should be empty.
         */
        void addEntry(const nostd::source_location location = nostd::source_location::current());

        /**
         * Creates an error string that
         * contains an error message and an
         * error stack.
         *
         * @return Error message and error stack.
         */
        std::string getError() const;

        /**
         * Returns initial error message that was
         * used to create this error.
         *
         * @return Initial error message.
         */
        std::string getInitialMessage() const;

        /**
         * Creates an error string, shows it on screen and also writes it to log.
         */
        void showError() const;

    protected:
        /**
         * Converts source_location instance to location information.
         *
         * @param location Source location instance.
         *
         * @return Location information.
         */
        static SourceLocationInfo sourceLocationToInfo(const nostd::source_location& location);

    private:
        /** Initial error message (string version). */
        std::string sMessage;

        /** Error stack. */
        std::vector<SourceLocationInfo> stack;
    };
} // namespace ne
