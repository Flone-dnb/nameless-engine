#pragma once

// STL.
#include <future>
#include <optional>
#include <chrono>
#include <vector>

namespace ne {
    /** Simple timer that can trigger a callback function on a timeout. */
    class Timer {
    public:
        Timer() = delete;

        /**
         * Constructor.
         *
         * @param bWaitForCallbacksToFinishOnDestruction Whether the timer needs to wait for all started
         * callback functions to finish or not (see @ref setCallbackForTimeout) in timer destructor.
         * Generally should be 'true', unless you're using the timer to call functions from other objects.
         * If you will not use @ref setCallbackForTimeout this parameter is ignored.
         */
        Timer(bool bWaitForCallbacksToFinishOnDestruction);

        /** Destructor. */
        ~Timer();

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        /**
         * Sets a function to be executed when the waiting
         * time is over (timeout).
         *
         * @param iTimeToWaitInMs Time this timer should wait (in milliseconds) until the callback is called.
         * @param callback Function to execute on timeout.
         * @param bIsLooping  Whether the timer should start again after a timeout or not.
         * If specified 'true', after the waiting time is over (timeout) the timer will automatically
         * restart itself and will start the waiting time again. There are 2 ways to stop a looping timer:
         * call @ref stop or destroy this object (destructor is called).
         *
         * @remark If the timer is currently running this call will update the callback to call on timeout.
         */
        void setCallbackForTimeout(
            long long iTimeToWaitInMs, const std::function<void()>& callback, bool bIsLooping = false);

        /**
         * Starts the timer.
         *
         * @remark If you want to add a callback function to be
         * executed on timeout see @ref setCallbackForTimeout.
         *
         * @remark If the timer is currently running it will be stopped (see @ref stop).
         */
        void start();

        /**
         * Stops the timer and timer looping (if was specified in @ref start).
         */
        void stop();

        /**
         * Returns the time that has passed since the timer was started (see @ref start).
         *
         * @return Empty if the @ref start was never called before or if the timer was stopped
         * (see @ref stop), otherwise time in milliseconds since the timer was started.
         *
         * @remark For looping timers (see @ref start) returns time since the beginning
         * of the current loop iteration. Each new loop will reset elapsed time to 0.
         *
         * @remark Note that if you call this function right after a call to @ref start
         * this function may return empty because the timer thread is not started yet.
         */
        std::optional<long long> getElapsedTimeInMs();

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

        /**
         * Time when the @ref start was called.
         * Not empty if @ref start was called.
         */
        std::pair<std::mutex, std::optional<std::chrono::steady_clock::time_point>> mtxTimeWhenStarted;

        /** Mutex for read/write operations on data that the timer thread is using. */
        std::mutex mtxTerminateTimerThread;

        /** Array of futures of started callback threads. */
        std::pair<std::mutex, std::vector<std::future<void>>> mtxFuturesForStartedCallbackThreads;

        /** Condition variable for timer thread termination. */
        std::condition_variable cvTerminateTimerThread;

        /** Whether the destructor was called or not. */
        std::atomic_flag bIsShuttingDown{};

        /** Whether the timer was explicitly stopped or not. */
        std::atomic_flag bIsStopRequested{};

        /** Time to wait until the callback is called. */
        long long iTimeToWaitInMs = 0;

        /** In destructor whether we need to wait for all started callback functions to finish or not. */
        bool bWaitForCallbacksToFinishOnDestruction = true;

        /** Whether the timer should restart itself upon a timeout or not. */
        bool bIsLooping = false;
    };
} // namespace ne
