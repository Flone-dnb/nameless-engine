#include "io/Logger.h"

// Std.
#include <ctime>
#include <fstream>

// Custom.
#include "misc/Globals.h"
#include "misc/ProjectPaths.h"

// External.
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "fmt/core.h"

namespace ne {
    Logger::~Logger() {
        if (iTotalWarningsProduced || iTotalErrorsProduced) {
            info(
                fmt::format(
                    "\n---------------------------------------------------\nTotal warnings produced: "
                    "{}.\nTotal errors produced: {}.",
                    iTotalWarningsProduced,
                    iTotalErrorsProduced),
                "");
        }
    }

    Logger& Logger::get() {
        static Logger logger;
        return logger;
    }

    void Logger::info(
        std::string_view sText, std::string_view sCategory, const nostd::source_location location) const {
        if (sCategory.empty()) {
            sCategory = sDefaultLogCategory;
        }
        pSpdLogger->info(fmt::format(
            "[{}] [{}, {}] {}",
            sCategory,
            std::filesystem::path(location.file_name()).filename().string(),
            location.line(),
            sText));
    }

    void Logger::warn(
        std::string_view sText, std::string_view sCategory, const nostd::source_location location) const {
        if (sCategory.empty()) {
            sCategory = sDefaultLogCategory;
        }
        pSpdLogger->warn(fmt::format(
            "[{}] [{}:{}] {}",
            sCategory,
            std::filesystem::path(location.file_name()).filename().string(),
            location.line(),
            sText));
        iTotalWarningsProduced += 1;
    }

    void Logger::error(
        std::string_view sText, std::string_view sCategory, const nostd::source_location location) const {
        if (sCategory.empty()) {
            sCategory = sDefaultLogCategory;
        }
        pSpdLogger->error(fmt::format(
            "[{}] [{}:{}] {}",
            sCategory,
            std::filesystem::path(location.file_name()).filename().string(),
            location.line(),
            sText));
        iTotalErrorsProduced += 1;
    }

    std::filesystem::path Logger::getDirectoryWithLogs() const { return sLoggerWorkingDirectory; }

    Logger::Logger() {
        auto sLoggerFilePath = ProjectPaths::getDirectoryForLogFiles();

        if (!std::filesystem::exists(sLoggerFilePath)) {
            std::filesystem::create_directories(sLoggerFilePath);
        }

        sLoggerWorkingDirectory = sLoggerFilePath;
        sLoggerFilePath /= getApplicationName() + "-" + getDateTime() + ".txt";

        removeOldestLogFiles(sLoggerWorkingDirectory);

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
#if defined(WIN32)
        const auto iError = localtime_s(&tm, &now);
        if (iError != 0) {
            get().error(fmt::format("failed to get localtime (error code {})", iError), "");
        }
#elif __linux__
        localtime_r(&now, &tm);
#else
        static_assert(false, "not implemented");
#endif

        return fmt::format("{}.{}_{}-{}-{}", 1 + tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    }

    void Logger::removeOldestLogFiles(const std::filesystem::path& sLogDirectory) {
        const auto directoryIterator = std::filesystem::directory_iterator(sLogDirectory);

        size_t iFileCount = 0;

        auto oldestTime = std::chrono::file_clock::now();
        std::filesystem::path oldestFilePath = "";

        for (auto const& entry : directoryIterator) {
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
