#include "misc/Timer.h"

// Custom.
#include "io/Logger.h"

// External.
#include "fmt/core.h"

namespace ne {
    Timer::Timer(bool bWaitForCallbacksToFinishOnDestruction) {
        this->bWaitForCallbacksToFinishOnDestruction = bWaitForCallbacksToFinishOnDestruction;
    }

    Timer::~Timer() {
        {
            std::scoped_lock guard(mtxTerminateTimerThread);
            if (!timerThreadFuture.has_value())
                return;

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

        {
            std::scoped_lock guard(mtxFuturesForStartedCallbackThreads.first);
            if (!mtxFuturesForStartedCallbackThreads.second.empty()) {
                const auto start = std::chrono::steady_clock::now();
                for (auto& future : mtxFuturesForStartedCallbackThreads.second) {
                    try {
                        if (future.valid()) {
                            future.get();
                        }
                    } catch (std::exception& ex) {
                        Logger::get().error(
                            fmt::format(
                                "timer's callback function thread (user code) has finished with the "
                                "following "
                                "exception: {}",
                                ex.what()),
                            "");
                    }
                }
                const auto end = std::chrono::steady_clock::now();
                const auto durationInMs =
                    static_cast<float>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) *
                    0.000001f;

                // Limit precision to 1 digit.
                std::stringstream durationStream;
                durationStream << std::fixed << std::setprecision(1) << durationInMs;

                if (durationInMs < 1.0f) {
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
                            "millisecond(s)",
                            durationStream.str()),
                        "");
                }
            }
        }
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

    void Timer::start() { // NOLINT: shadowing member variable
        if (bIsShuttingDown.test())
            return;

        if (timerThreadFuture.has_value()) {
            stop();
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

    void Timer::stop() {
        {
            std::scoped_lock guard(mtxTerminateTimerThread);

            bIsStopRequested.test_and_set();

            // Notify timer thread if it's running.
            if (!timerThreadFuture.has_value())
                return;

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
            } else {
                // No callback was set, being used as a regular timer.
                return !bIsStopRequested.test();
            }
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
                if (bWaitForCallbacksToFinishOnDestruction) {
                    std::scoped_lock guard1(mtxFuturesForStartedCallbackThreads.first);
                    mtxFuturesForStartedCallbackThreads.second.push_back(std::async(
                        std::launch::async, [callback = callbackForTimeout.value()]() { callback(); }));
                } else {
                    auto handle = std::thread([callback = callbackForTimeout.value()]() { callback(); });
                    handle.detach();
                }
            }
        } while (!bIsShuttingDown.test() && !bIsStopRequested.test() && bIsLooping);
    }
} // namespace ne
