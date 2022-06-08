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
            timerThreadFuture->get();
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

        std::scoped_lock guard(mtxTerminateTimerThread);
        bIsStopped.clear();
        timerThreadFuture = std::async(
            std::launch::async, &Timer::timerThread, this, std::chrono::milliseconds(iTimeToWaitInMs));
    }

    void Timer::stop() {
        std::scoped_lock guard(mtxTerminateTimerThread);
        if (!timerThreadFuture.has_value())
            return;

        // Notify timer thread if it's running.
        bIsStopped.test_and_set();
        cvTerminateTimerThread.notify_one();
    }

    Timer::Timer(TimerManager* pTimerManager) { this->pTimerManager = pTimerManager; }

    void Timer::timerThread(std::chrono::milliseconds timeToWaitInMs) {
        std::unique_lock guard(mtxTerminateTimerThread);
        cvTerminateTimerThread.wait_for(
            guard, timeToWaitInMs, [this] { return bIsShuttingDown.test() || bIsStopped.test(); });

        if (!bIsShuttingDown.test() && !bIsStopped.test() && callbackForTimeout.has_value()) {
            callbackForTimeout->operator()();
        }
    }
} // namespace ne
