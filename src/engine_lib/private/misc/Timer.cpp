#include "misc/Timer.h"

// Custom.
#include "io/Logger.h"

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
                std::format("a timer thread has finished with the following exception: {}", ex.what()), "");
        }

        {
            std::scoped_lock guard(mtxFuturesForStartedCallbackThreads.first);
            if (bWaitForCallbacksToFinishOnDestruction &&
                !mtxFuturesForStartedCallbackThreads.second.empty()) {
                const auto start = std::chrono::steady_clock::now();
                for (auto& future : mtxFuturesForStartedCallbackThreads.second) {
                    try {
                        if (future.valid()) {
                            future.get();
                        }
                    } catch (std::exception& ex) {
                        Logger::get().error(
                            std::format(
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

                if (durationInMs < 1.0f) {
                    // Information.
                    Logger::get().info(
                        std::format(
                            "timer has finished waiting for started callback functions to finish, took {} "
                            "millisecond",
                            durationInMs),
                        "");
                } else {
                    // Warning.
                    Logger::get().warn(
                        std::format(
                            "timer has finished waiting for started callback functions to finish, took {} "
                            "millisecond(-s)",
                            durationInMs),
                        "");
                }
            }
        }
    }

    void Timer::setCallbackForTimeout(const std::function<void()>& callback) {
        std::scoped_lock guard(mtxTerminateTimerThread);
        callbackForTimeout = callback;
    }

    void Timer::start(long long iTimeToWaitInMs, bool bIsLooping) { // NOLINT: shadowing member variable
        if (bIsShuttingDown.test())
            return;

        if (timerThreadFuture.has_value()) {
            stop();
        }

        std::scoped_lock guard(mtxTerminateTimerThread);
        this->bIsLooping = bIsLooping;
        bIsStopRequested.clear();
        timerThreadFuture = std::async(
            std::launch::async, &Timer::timerThread, this, std::chrono::milliseconds(iTimeToWaitInMs));
    }

    void Timer::stop() {
        {
            std::scoped_lock guard(mtxTerminateTimerThread);
            if (!timerThreadFuture.has_value())
                return;

            // Notify timer thread if it's running.
            bIsStopRequested.test_and_set();
            cvTerminateTimerThread.notify_one();
        }

        try {
            if (timerThreadFuture->valid()) {
                timerThreadFuture->get();
            }
        } catch (std::exception& ex) {
            Logger::get().error(
                std::format("a timer thread has finished with the following exception: {}", ex.what()), "");
        }
    }

    bool Timer::isRunning() {
        std::scoped_lock guard(mtxTerminateTimerThread);
        if (!timerThreadFuture.has_value())
            return false;

        if (!timerThreadFuture->valid()) {
            return false;
        }

        const auto status = timerThreadFuture->wait_for(std::chrono::nanoseconds(0));

        return status != std::future_status::ready;
    }

    void Timer::timerThread(std::chrono::milliseconds timeToWaitInMs) {
        do {
            std::unique_lock guard(mtxTerminateTimerThread);
            cvTerminateTimerThread.wait_for(
                guard, timeToWaitInMs, [this] { return bIsShuttingDown.test() || bIsStopRequested.test(); });

            if (!bIsShuttingDown.test() && !bIsStopRequested.test() && callbackForTimeout.has_value()) {
                std::scoped_lock guard1(mtxFuturesForStartedCallbackThreads.first);
                mtxFuturesForStartedCallbackThreads.second.push_back(std::async(
                    std::launch::async, [callback = callbackForTimeout.value()]() { callback(); }));
            }
        } while (!bIsShuttingDown.test() && !bIsStopRequested.test() && bIsLooping);
    }
} // namespace ne
