#pragma once

// STL.
#include <string_view>
#include <vector>
#include <source_location>

#if defined(WIN32)
#include <Windows.h>
#endif

namespace ne {
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
            std::string_view sMessage, const std::source_location location = std::source_location::current());

#if defined(WIN32)
        /**
         * Construct a new Error object from HRESULT.
         *
         * @param hResult   HRESULT that contains an error.
         * @param location  Should be empty.
         */
        Error(const HRESULT hResult, const std::source_location location = std::source_location::current());

        /**
         * Construct a new Error object.
         *
         * @param errorCode Error code returned by GetLastError().
         * @param location  Should be empty.
         */
        Error(unsigned long errorCode, const std::source_location location = std::source_location::current());
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
        void addEntry(const std::source_location location = std::source_location::current());

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
        /** Initial error message (string version). */
        std::string sMessage;
        /** Error stack. */
        std::vector<std::source_location> stack;
    };
} // namespace ne