#pragma once

// STL.
#include <string_view>
#include <vector>
#include <source_location>

namespace dxe {
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
        Error(std::string_view sMessage,
              const std::source_location location = std::source_location::current());

        /**
         * Construct a new Error object.
         *
         * @param errorCode Error code returned by GetLastError().
         * @param location  Should be empty.
         */
        Error(unsigned long errorCode, const std::source_location location = std::source_location::current());

        Error() = delete;
        Error(const Error &) = delete;
        Error &operator=(const Error &) = delete;
        virtual ~Error() = default;

        /**
         * Move constructor.
         *
         * @param other other object.
         */
        Error(Error &&other) = default;
        /**
         * Move assignment.
         *
         * @param other other object.
         *
         * @return Result of move assignment.
         */
        Error &operator=(Error &&other) = default;

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
         * Creates an error string
         * and shows it on screen.
         */
        void showError() const;

    protected:
        /** Initial error message. */
        std::string sMessage;
        /** Error stack. */
        std::vector<std::source_location> stack;
    };
} // namespace dxe