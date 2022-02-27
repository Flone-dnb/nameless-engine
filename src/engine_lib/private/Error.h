#pragma once

// STL.
#include <string_view>
#include <vector>
#include <source_location>

namespace dxe {
    /**
     * @brief Helper class to store error messages.
     */
    class Error {
    public:
        /**
         * @brief Construct a new Error object.
         *
         * @param sMessage  Error message to show.
         * @param location  Should be empty.
         */
        Error(std::string_view sMessage,
              const std::source_location location = std::source_location::current());

        /**
         * @brief Construct a new Error object.
         *
         * @param errorCode Error code returned by GetLastError().
         * @param location  Should be empty.
         */
        Error(unsigned long errorCode, const std::source_location location = std::source_location::current());

        Error() = delete;
        Error(const Error &) = delete;
        Error &operator=(const Error &) = delete;
        virtual ~Error() = default;

        Error(Error &&other) noexcept;
        Error &operator=(Error &&other) noexcept;

        /**
         * @brief Adds an entry to the error stack.
         *
         * @param location  Should be empty.
         */
        void addEntry(const std::source_location location = std::source_location::current());

        /**
         * @brief Creates an error string that
         * contains an error message and an
         * error stack.
         */
        std::string getError() const;

        /**
         * @brief Creates an error string
         * and shows it on screen.
         */
        void showError() const;

    protected:
        void swap(Error &&other);

        std::string sMessage;
        std::vector<std::source_location> stack;
    };
} // namespace dxe