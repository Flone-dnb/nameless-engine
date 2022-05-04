#include "io/Logger.h"

// Std.
#include <ctime>
#include <format>
#include <fstream>

// Custom.
#include "misc/Globals.h"

// External.
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace ne {
    Logger &Logger::get() {
        static Logger logger;
        return logger;
    }

    void Logger::info(std::string_view sText, const std::source_location location) const {
        pSpdLogger->info(std::format(
            "[{}:{}] {}",
            std::filesystem::path(location.file_name()).filename().string(),
            location.line(),
            sText));
    }

    void Logger::warn(std::string_view sText, const std::source_location location) const {
        pSpdLogger->warn(std::format(
            "[{}:{}] {}",
            std::filesystem::path(location.file_name()).filename().string(),
            location.line(),
            sText));
    }

    void Logger::error(std::string_view sText, const std::source_location location) const {
        pSpdLogger->error(std::format(
            "[{}:{}] {}",
            std::filesystem::path(location.file_name()).filename().string(),
            location.line(),
            sText));
    }

    std::filesystem::path Logger::getDirectoryWithLogs() const { return sLoggerWorkingDirectory; }

    Logger::Logger() {
        auto sLoggerFilePath = getBaseDirectoryForConfigs();
        sLoggerFilePath += getApplicationName();

        // Create base directory with application name.
        if (!std::filesystem::exists(sLoggerFilePath)) {
            std::filesystem::create_directory(sLoggerFilePath);
        }

        // Create directory for logs.
        sLoggerFilePath /= sLogsDirectoryName;
        if (!std::filesystem::exists(sLoggerFilePath)) {
            std::filesystem::create_directory(sLoggerFilePath);
        }

        sLoggerWorkingDirectory = sLoggerFilePath;
        sLoggerFilePath /= getApplicationName() + "-" + getDateTime() + ".txt";

        removeOldestLogFileIfMaxLogFiles(sLoggerWorkingDirectory);

        if (!std::filesystem::exists(sLoggerFilePath)) {
            std::ofstream logFile(sLoggerFilePath);
            logFile.close();
        }

        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(sLoggerFilePath.string(), true);

        pSpdLogger =
            std::unique_ptr<spdlog::logger>(new spdlog::logger("MainLogger", {consoleSink, fileSink}));
        pSpdLogger->set_pattern("[%H:%M:%S] [%^%l%$] %v");
    }

    std::string Logger::getDateTime() {
        const time_t now = time(nullptr);

        tm tm{};
        localtime_s(&tm, &now);

        return std::format(
            "{}.{}_{}-{}-{}",
            1 + tm.tm_mon,
            tm.tm_mday,
            tm.tm_hour, // NOLINT
            tm.tm_min,
            tm.tm_sec); // NOLINT
    }

    void Logger::removeOldestLogFileIfMaxLogFiles(const std::filesystem::path &sLogDirectory) {
        const auto directoryIterator = std::filesystem::directory_iterator(sLogDirectory);

        size_t iFileCount = 0;

        auto oldestTime = std::chrono::file_clock::now();
        std::filesystem::path oldestFilePath = "";

        for (auto const &entry : directoryIterator) {
            if (entry.is_regular_file()) {
                iFileCount += 1;
            }

            auto fileLastWriteTime = entry.last_write_time();
            if (fileLastWriteTime < oldestTime) {
                oldestTime = fileLastWriteTime;
                oldestFilePath = entry.path();
            }
        }

        if (iFileCount < iMaxLogFiles) {
            return;
        }

        if (oldestFilePath.empty()) {
            return;
        }

        std::filesystem::remove(oldestFilePath);
    }
} // namespace ne
