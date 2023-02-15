#include "misc/Timer.h"

// Custom.
#include "io/Logger.h"

// External.
#include "fmt/core.h"

namespace ne {
    Timer::Timer(bool bWarnAboutWaitingForCallbackTooLong) {
        this->bWarnAboutWaitingForCallbackTooLong = bWarnAboutWaitingForCallbackTooLong;
    }

    Timer::~Timer() {
        {
            std::scoped_lock guard(mtxTerminateTimerThread);
            if (!timerThreadFuture.has_value()) {
                return;
            }

            // Notify timer thread if it's running.
            bIsShuttingDown.test_and_set();
            cvTerminateTimerThread.notify_one();
        }

        try {
            if (timerThreadFuture->valid()) {
                timerThreadFuture->get();
            }
        } catch (std::exception& ex) {
            Logger::get().error(
                fmt::format("a timer thread has finished with the following exception: {}", ex.what()), "");
        }

        waitForRunningCallbackThreadsToFinish();
    }

    void Timer::setCallbackForTimeout(
        long long iTimeToWaitInMs, // NOLINT: shadowing member variable
        const std::function<void()>& callback,
        bool bIsLooping) { // NOLINT: shadowing member variable
        std::scoped_lock guard(mtxTerminateTimerThread);
        callbackForTimeout = callback;
        this->iTimeToWaitInMs = iTimeToWaitInMs;
        this->bIsLooping = bIsLooping;
    }

    void Timer::start(bool bWaitForRunningCallbackThreadsToFinish) {
        if (bIsShuttingDown.test()) {
            return;
        }

        if (timerThreadFuture.has_value()) {
            stop(bWaitForRunningCallbackThreadsToFinish);
        }

        std::scoped_lock guard(mtxTerminateTimerThread);
        bIsStopRequested.clear();

        if (callbackForTimeout.has_value()) {
            // Use a separate thread to wait for timeout.
            timerThreadFuture = std::async(
                std::launch::async, &Timer::timerThread, this, std::chrono::milliseconds(iTimeToWaitInMs));
        } else {
            // Mark start time. No need to sleep.
            std::scoped_lock timeGuard(mtxTimeWhenStarted.first);
            mtxTimeWhenStarted.second = std::chrono::steady_clock::now();
        }
    }

    void Timer::stop(bool bWaitForRunningCallbackThreadsToFinish) {
        {
            std::scoped_lock guard(mtxTerminateTimerThread);

            bIsStopRequested.test_and_set();

            // Notify timer thread if it's running.
            if (!timerThreadFuture.has_value()) {
                return;
            }

            cvTerminateTimerThread.notify_one();

            if (bWaitForRunningCallbackThreadsToFinish) {
                waitForRunningCallbackThreadsToFinish();
            }
        }

        try {
            if (timerThreadFuture->valid()) {
                timerThreadFuture->get();
            }
        } catch (std::exception& ex) {
            Logger::get().error(
                fmt::format("a timer thread has finished with the following exception: {}", ex.what()), "");
        }

        timerThreadFuture = {};
    }

    std::optional<long long> Timer::getElapsedTimeInMs() {
        std::scoped_lock guard(mtxTimeWhenStarted.first);

        if (!mtxTimeWhenStarted.second.has_value()) {
            return {};
        }

        if (bIsStopRequested.test()) {
            return {};
        }

        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now() - mtxTimeWhenStarted.second.value()).count();
    }

    bool Timer::isRunning() {
        std::scoped_lock guard(mtxTerminateTimerThread);
        if (!timerThreadFuture.has_value()) {
            // Timer thread is not running.
            if (callbackForTimeout.has_value()) {
                // Callback was set, so the timer thread is used.
                return false;
            }

            // No callback was set, being used as a regular timer.
            return !bIsStopRequested.test();
        }

        if (!timerThreadFuture->valid()) {
            return false;
        }

        const auto status = timerThreadFuture->wait_for(std::chrono::nanoseconds(0));

        return status != std::future_status::ready;
    }

    void Timer::timerThread(std::chrono::milliseconds timeToWaitInMs) {
        do {
            {
                // Mark start time.
                std::scoped_lock timeGuard(mtxTimeWhenStarted.first);
                mtxTimeWhenStarted.second = std::chrono::steady_clock::now();
            }
            std::unique_lock guard(mtxTerminateTimerThread);
            cvTerminateTimerThread.wait_for(
                guard, timeToWaitInMs, [this] { return bIsShuttingDown.test() || bIsStopRequested.test(); });

            if (!bIsShuttingDown.test() && !bIsStopRequested.test() && callbackForTimeout.has_value()) {
                std::scoped_lock guard1(mtxStartedCallbackThreads.first);
                mtxStartedCallbackThreads.second.push_back(
                    std::thread([&]() { callbackForTimeout->operator()(); }));
            }
        } while (!bIsShuttingDown.test() && !bIsStopRequested.test() && bIsLooping);
    }

    void Timer::waitForRunningCallbackThreadsToFinish() {
        std::scoped_lock guard(mtxStartedCallbackThreads.first);
        if (mtxStartedCallbackThreads.second.empty()) {
            return;
        }

        const auto start = std::chrono::steady_clock::now();
        for (auto& thread : mtxStartedCallbackThreads.second) {
            try {
                thread.join();
            } catch (std::exception& ex) {
                Logger::get().error(
                    fmt::format(
                        "timer's callback function thread (user code) has finished with the "
                        "following exception: {}",
                        ex.what()),
                    "");
            }
        }
        const auto end = std::chrono::steady_clock::now();
        const auto durationInMs =
            static_cast<float>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) *
            0.000001F;

        // Limit precision to 1 digit.
        std::stringstream durationStream;
        durationStream << std::fixed << std::setprecision(1) << durationInMs;

        if (durationInMs < 1.0F || !bWarnAboutWaitingForCallbackTooLong) {
            // Information.
            Logger::get().info(
                fmt::format(
                    "timer has finished waiting for started callback functions to finish, took {} "
                    "millisecond",
                    durationStream.str()),
                "");
        } else {
            // Warning.
            Logger::get().warn(
                fmt::format(
                    "timer has finished waiting for started callback functions to finish, took {} "
                    "millisecond(s)\n"
                    "(hint: specify `bWarnAboutWaitingForCallbackTooLong` as `false` to timer "
                    "constructor to convert this message category from `warning` to `info`)",
                    durationStream.str()),
                "");
        }

        mtxStartedCallbackThreads.second.clear();
    }
} // namespace ne
