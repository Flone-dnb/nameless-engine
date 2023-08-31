#include "misc/Timer.h"

// Standard.
#include <format>

// Custom.
#include "io/Logger.h"
#include "game/GameManager.h"

namespace ne {
    Timer::Timer(const std::string& sTimerName) { this->sTimerName = sTimerName; }

    void Timer::setCallbackValidator(const std::function<bool(size_t)>& validator) {
        if (isRunning()) [[unlikely]] {
            Logger::get().error(std::format(
                "\"{}\" timer is unable to set a callback validator while the timer is running", sTimerName));
            return;
        }

        this->callbackValidator = validator;
    }

    void Timer::setEnable(bool bEnable) {
        std::scoped_lock guard(mtxTerminateTimerThread);
        bIsEnabled = bEnable;
    }

    Timer::~Timer() {
        {
            // Make sure the timer thread is running.
            std::scoped_lock guard(mtxTerminateTimerThread);
            if (!timerThreadFuture.has_value()) {
                return;
            }

            // Notify the timer thread if it's still running.
            bIsShuttingDown.test_and_set();
            cvTerminateTimerThread.notify_one();
        }

        // Wait for the timer thread to finish.
        try {
            timerThreadFuture->get();
        } catch (std::exception& ex) {
            Logger::get().error(std::format(
                "\"{}\" timer thread has finished with the following exception: {}", sTimerName, ex.what()));
        }
    }

    void Timer::setCallbackForTimeout(
        long long iTimeToWaitInMs, // NOLINT: shadowing member variable
        const std::function<void()>& callback,
        bool bIsLooping) { // NOLINT: shadowing member variable
        if (isRunning()) [[unlikely]] {
            Logger::get().error(std::format(
                "\"{}\" timer is unable to set a callback for timeout while the timer is running",
                sTimerName));
            return;
        }

        // Make sure that time to wait is positive.
        if (iTimeToWaitInMs < 0) [[unlikely]] {
            Logger::get().error(std::format(
                "\"{}\" timer is unable to set a callback for timeout because the specified time to "
                "wait ({}) is negative",
                sTimerName,
                iTimeToWaitInMs));
            return;
        }

        std::scoped_lock guard(mtxTerminateTimerThread);
        callbackForTimeout = callback;
        this->iTimeToWaitInMs = iTimeToWaitInMs;
        this->bIsLooping = bIsLooping;
    }

    void Timer::start() {
        {
            // Make sure the timer is enabled.
            std::scoped_lock guard(mtxTerminateTimerThread);
            if (!bIsEnabled) {
                Logger::get().error(std::format(
                    "\"{}\" timer was requested to start while disabled, timer will not be started",
                    sTimerName));
                return;
            }

            iStartCount += 1;
        }

        if (bIsShuttingDown.test()) {
            return;
        }

        if (timerThreadFuture.has_value()) {
            // Stop the timer thread.
            stop();
        }

        std::scoped_lock guard(mtxTerminateTimerThread);
        bIsStopRequested.clear();

        if (callbackForTimeout.has_value()) {
            // Use a separate thread to wait for timeout.
            timerThreadFuture = std::async(
                std::launch::async, &Timer::timerThread, this, std::chrono::milliseconds(iTimeToWaitInMs));
        } else {
            // Mark start time. No need to use a separate thread.
            std::scoped_lock timeGuard(mtxTimeWhenStarted.first);
            mtxTimeWhenStarted.second = std::chrono::steady_clock::now();
            bIsRunning = true;
        }
    }

    void Timer::stop(bool bDisableTimer) {
        {
            std::scoped_lock guard(mtxTerminateTimerThread);

            // Save elapsed time.
            if (!mtxTimeWhenStarted.second.has_value()) {
                elapsedTimeWhenStopped = {};
            } else {
                using namespace std::chrono;
                elapsedTimeWhenStopped =
                    duration_cast<milliseconds>(steady_clock::now() - mtxTimeWhenStarted.second.value())
                        .count();
            }

            bIsEnabled = !bDisableTimer;
            bIsStopRequested.test_and_set();

            // Notify the timer thread if it's running.
            if (!timerThreadFuture.has_value()) {
                // No callback was set, being used as a regular timer.
                bIsRunning = false;
                return;
            }

            cvTerminateTimerThread.notify_one();
        }

        // Wait for the timer thread to finish.
        try {
            timerThreadFuture->get();
        } catch (std::exception& ex) {
            Logger::get().error(std::format(
                "\"{}\" timer thread has finished with the following exception: {}", sTimerName, ex.what()));
        }

        timerThreadFuture = {};

        bIsRunning = false;
    }

    std::optional<long long> Timer::getElapsedTimeInMs() {
        {
            // If was stopped return time when was stopped.
            std::scoped_lock guard(mtxTerminateTimerThread);
            if (bIsStopRequested.test()) {
                return elapsedTimeWhenStopped;
            }
        }

        std::scoped_lock guard(mtxTimeWhenStarted.first);

        if (!mtxTimeWhenStarted.second.has_value()) {
            return {};
        }

        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now() - mtxTimeWhenStarted.second.value()).count();
    }

    std::string Timer::getName() const { return sTimerName; }

    size_t Timer::getStartCount() {
        std::scoped_lock guard(mtxTerminateTimerThread);
        return iStartCount;
    }

    bool Timer::isRunning() {
        std::scoped_lock guard(mtxTerminateTimerThread);
        return bIsRunning;
    }

    bool Timer::isStopped() {
        std::scoped_lock guard(mtxTerminateTimerThread);
        return bIsStopRequested.test();
    }

    bool Timer::isEnabled() {
        std::scoped_lock guard(mtxTerminateTimerThread);
        return bIsEnabled;
    }

    void Timer::timerThread(std::chrono::milliseconds timeToWaitInMs) {
        using namespace std::chrono;

        {
            std::scoped_lock terminationGuard(mtxTerminateTimerThread);
            bIsRunning = true;
        }

        do {
            {
                // Mark start time.
                std::scoped_lock timeGuard(mtxTimeWhenStarted.first);
                mtxTimeWhenStarted.second = steady_clock::now();
            }

            do {
                // Wait until timeout or stopped or shutdown.
                std::unique_lock terminationGuard(mtxTerminateTimerThread);
                cvTerminateTimerThread.wait_for(terminationGuard, timeToWaitInMs, [this] {
                    return bIsShuttingDown.test() || bIsStopRequested.test();
                });

                if (bIsShuttingDown.test() || bIsStopRequested.test()) {
                    bIsRunning = false;
                    return;
                }

                // Check how much time has passed.
                std::scoped_lock timeGuard(mtxTimeWhenStarted.first);

                const long long iMillisecondsPassed =
                    duration_cast<milliseconds>(steady_clock::now() - mtxTimeWhenStarted.second.value())
                        .count();
                const long long iMillisecondsLeft = timeToWaitInMs.count() - iMillisecondsPassed;
                if (iMillisecondsLeft > 0) {
                    // Seems to be a spurious wakeup.
                    timeToWaitInMs = std::chrono::milliseconds(iMillisecondsLeft);
                    continue;
                }

                // Enough time has passed.

                // Releasing termination mutex here on purpose (because we don't need to hold it anymore).
                // TODO: should we just use a dummy mutex for `wait_for`?
            } while (false);

            if (callbackForTimeout.has_value()) {
                // Get game manager to submit a deferred task.
                const auto pGameManager = GameManager::get();
                if (pGameManager == nullptr) [[unlikely]] {
                    Logger::get().error(std::format(
                        "timer \"{}\" is unable to start the callback because the game manager is "
                        "nullptr",
                        sTimerName));
                    std::scoped_lock terminationGuard(mtxTerminateTimerThread);
                    bIsRunning = false;
                    return;
                }

                if (pGameManager->isBeingDestroyed()) [[unlikely]] {
                    Logger::get().error(std::format(
                        "timer \"{}\" is unable to start the callback because the game manager is being "
                        "destroyed",
                        sTimerName));
                    std::scoped_lock terminationGuard(mtxTerminateTimerThread);
                    bIsRunning = false;
                    return;
                }

                // Submit a deferred task.
                if (callbackValidator.has_value()) {
                    pGameManager->addDeferredTask([iStartCount = iStartCount,
                                                   validator = callbackValidator.value(),
                                                   callbackForTimeout = callbackForTimeout.value()]() {
                        if (validator(iStartCount)) {
                            callbackForTimeout();
                        }
                    });
                } else {
                    pGameManager->addDeferredTask(callbackForTimeout.value());
                }
            }
        } while (!bIsShuttingDown.test() && !bIsStopRequested.test() && bIsLooping);

        std::scoped_lock terminationGuard(mtxTerminateTimerThread);
        bIsRunning = false;
    }
} // namespace ne
