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
         * @warning If you are have a callback function (class member function) set to this timer,
         * make sure the timer is stopped (@ref stop) before the callback owner (object that owns member
         * function) is deleted or if you no longer need this timer.
         *
         * @note If you want to add a callback function to be
         * executed on timeout see @ref setCallbackForTimeout.
         */
        void start(long long iTimeToWaitInMs);

        /**
         * Stops the timer.
         *
         * @note If a callback function was set for this timer
         * and the timer is still running, the timer will be stopped
         * and the callback function will not be called.
         */
        void stop();

    private:
        friend class TimerManager;

        /**
         * Constructor.
         *
         * @param pTimerManager Owner.
         */
        Timer(TimerManager* pTimerManager);

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

        /** Do not delete. Owner of this timer. */
        TimerManager* pTimerManager = nullptr;
    };
} // namespace ne
