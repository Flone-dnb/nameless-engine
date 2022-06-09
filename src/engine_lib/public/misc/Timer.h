#pragma once

// STL.
#include <future>
#include <optional>
#include <chrono>

namespace ne {
    /** Simple timer that can trigger a callback function on a timeout. */
    class Timer {
    public:
        Timer() = default;

        /** Destructor. */
        ~Timer();

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        /**
         * Sets a function to be executed when the waiting
         * time is over (timeout).
         *
         * @param callback Function to execute on timeout.
         */
        void setCallbackForTimeout(const std::function<void()>& callback);

        /**
         * Starts the timer.
         *
         * @param iTimeToWaitInMs Time this timer should wait (in milliseconds).
         *
         * @note If you want to add a callback function to be
         * executed on timeout see @ref setCallbackForTimeout.
         *
         * @note If the timer is currently running it will be stopped
         * (this might block, see @ref stop), to start a new one.
         */
        void start(long long iTimeToWaitInMs);

        /**
         * Stops the timer and blocks until timer thread is finished.
         *
         * If a callback function was set for this timer
         * and the timer is still running, there are 2 possible outcomes:
         * - (if the callback was not started yet) the function will block until timer thread
         * is finished (without calling callback due to stop request) - should be almost immediate,
         * - (if the callback is started) the function will block until timer thread
         * and callback are finished.
         */
        void stop();

        /**
         * Whether this timer is running (started) or not (finished/not started).
         *
         * @return 'true' if currently running, 'false' otherwise.
         */
        bool isRunning();

    private:
        /**
         * Timer thread that waits until a timeout or a shutdown.
         *
         * @param timeToWaitInMs Time this thread should wait.
         */
        void timerThread(std::chrono::milliseconds timeToWaitInMs);

        /** Future of the waiting thread. */
        std::optional<std::future<void>> timerThreadFuture;

        /** Function to call on timeout. */
        std::optional<std::function<void()>> callbackForTimeout;

        /** Mutex for using condition variable @ref cvTerminateTimerThread. */
        std::mutex mtxTerminateTimerThread;

        /** Condition variable for timer thread termination. */
        std::condition_variable cvTerminateTimerThread;

        /** Whether the destructor was called or not. */
        std::atomic_flag bIsShuttingDown{};

        /** Whether the timer was explicitly stopped or not. */
        std::atomic_flag bIsStopped{};
    };
} // namespace ne
