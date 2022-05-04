#pragma once

// Std.
#include <memory>
#include <filesystem>
#include <source_location>

namespace spdlog {
    class logger;
}

namespace ne {
    /**
     * Logs to file and console.
     */
    class Logger {
    public:
        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;
        virtual ~Logger() = default;

        /**
         * Returns a reference to the logger instance.
         * If no instance was created yet, this function will create it
         * and return a reference to it.
         *
         * @return Reference to the logger instance.
         */
        static Logger &get();

        /**
         * Add text to console and log file using "info" category.
         * The text message will be appended with the file name and the line it was called from.
         *
         * @param sText    Text to write to log.
         * @param location Should not be passed explicitly.
         */
        void info(
            std::string_view sText,
            const std::source_location location = std::source_location::current()) const;

        /**
         * Add text to console and log file using "warning" category.
         * The text message will be appended with the file name and the line it was called from.
         *
         * @param sText  Text to write to log.
         * @param location Should not be passed explicitly.
         */
        void warn(
            std::string_view sText,
            const std::source_location location = std::source_location::current()) const;

        /**
         * Add text to console and log file using "error" category.
         * The text message will be appended with the file name and the line it was called from.
         *
         * @param sText  Text to write to log.
         * @param location Should not be passed explicitly.
         */
        void error(
            std::string_view sText,
            const std::source_location location = std::source_location::current()) const;

        /**
         * Returns the directory that contains all logs.
         *
         * @return Directory for logs.
         */
        std::filesystem::path getDirectoryWithLogs() const;

    private:
        Logger();

        /**
         * Returns current date and time in format "month.day_hour-minute-second".
         *
         * @return Date and time string.
         */
        static std::string getDateTime();

        static void removeOldestLogFileIfMaxLogFiles(const std::filesystem::path &sLogDirectory);

        /**
         * Spdlog logger.
         */
        std::unique_ptr<spdlog::logger> pSpdLogger = nullptr;

        /**
         * Directory that is used to create logs.
         */
        std::filesystem::path sLoggerWorkingDirectory;

        /**
         * Maximum amount of log files in the logger directory.
         * If the logger directory contains this amount of log files,
         * the oldest log file will be removed to create a new one.
         */
        inline static size_t iMaxLogFiles = 10;
    };
} // namespace ne
