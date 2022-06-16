#pragma once

// STL.
#include <future>
#include <optional>
#include <chrono>

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
         * @param callback Function to execute on timeout.
         *
         * @remark If the timer is currently running this call will update the callback to call on timeout.
         */
        void setCallbackForTimeout(const std::function<void()>& callback);

        /**
         * Starts the timer.
         *
         * @param iTimeToWaitInMs Time this timer should wait (in milliseconds).
         * @param bIsLooping      Whether the timer should start again after a timeout or not.
         * If specified 'true', after the waiting time is over (timeout) the timer will automatically
         * restart itself and will start the waiting time again. There are 2 ways to stop a looping timer:
         * call @ref stop or destroy this object (destructor is called).
         *
         * @note If you want to add a callback function to be
         * executed on timeout see @ref setCallbackForTimeout.
         *
         * @note If the timer is currently running it will be stopped
         * (this might block, see @ref stop), to start a new one.
         */
        void start(long long iTimeToWaitInMs, bool bIsLooping = false);

        /**
         * Stops the timer and timer looping (if was specified in @ref start).
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

        /** Array of futures of started callback threads. */
        std::pair<std::mutex, std::vector<std::future<void>>> mtxFuturesForStartedCallbackThreads;

        /** Condition variable for timer thread termination. */
        std::condition_variable cvTerminateTimerThread;

        /** Whether the destructor was called or not. */
        std::atomic_flag bIsShuttingDown{};

        /** Whether the timer was explicitly stopped or not. */
        std::atomic_flag bIsStopRequested{};

        /** In destructor whether we need to wait for all started callback functions to finish or not. */
        bool bWaitForCallbacksToFinishOnDestruction = true;

        /** Whether the timer should restart itself upon a timeout or not. */
        bool bIsLooping = false;
    };
} // namespace ne
