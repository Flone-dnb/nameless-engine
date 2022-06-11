#include "misc/Timer.h"

// Custom.
#include "io/Logger.h"

namespace ne {
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
    }

    void Timer::setCallbackForTimeout(const std::function<void()>& callback) {
        std::scoped_lock guard(mtxTerminateTimerThread);
        callbackForTimeout = callback;
    }

    void Timer::start(long long iTimeToWaitInMs) {
        if (bIsShuttingDown.test())
            return;

        if (timerThreadFuture.has_value()) {
            stop();
        }

        std::scoped_lock guard(mtxTerminateTimerThread);
        bIsStopped.clear();
        timerThreadFuture = std::async(
            std::launch::async, &Timer::timerThread, this, std::chrono::milliseconds(iTimeToWaitInMs));
    }

    void Timer::stop() {
        {
            std::scoped_lock guard(mtxTerminateTimerThread);
            if (!timerThreadFuture.has_value())
                return;

            // Notify timer thread if it's running.
            bIsStopped.test_and_set();
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
        bool bRunCallback = false;

        {
            std::unique_lock guard(mtxTerminateTimerThread);
            cvTerminateTimerThread.wait_for(
                guard, timeToWaitInMs, [this] { return bIsShuttingDown.test() || bIsStopped.test(); });

            if (!bIsShuttingDown.test() && !bIsStopped.test() && callbackForTimeout.has_value()) {
                bRunCallback = true;
            }
        }

        if (bRunCallback) {
            callbackForTimeout->operator()();
        }
    }
} // namespace ne
