#pragma once

// Standard.
#include <future>
#include <optional>
#include <chrono>
#include <vector>

namespace ne {
    class Node;

    /** Simple timer that can trigger a callback function on a timeout. */
    class Timer {
    public:
        Timer() = default;

        /**
         * Constructor.
         *
         * @param bWarnAboutWaitingForCallbackTooLong In timer destructor, whether the timer should log
         * a warning message if waiting for started callback to finish is taking too long or not.
         */
        Timer(bool bWarnAboutWaitingForCallbackTooLong);

        /** Destructor. */
        ~Timer();

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        /**
         * Sets a function to be executed when the waiting time is over (timeout).
         *
         * Example:
         * @code
         * class GrenadeNode : public MeshNode
         * {
         * public:
         *     // ...
         *
         *     void throw(){
         *         explodeTimer.setCallbackForTimeout(3000, [&]() { explode(); });
         *         explodeTimer.start();
         *     }
         * private:
         *     void explode(){
         *         // ...
         *     }
         *
         *     Timer explodeTimer;
         * }
         * @endcode
         *
         * @remark If the timer is currently running with some other callback setup earlier we will
         * use @ref stop to stop the timer and setup a new callback.
         *
         * @remark If the callback function is your node's member function, in this callback function
         * you don't need to worry about node being destroyed/deleted, the timer makes sure that the node
         * will not be deleted while the callback is running.
         *
         * @remark Although the timer will wait for the running callback function to finish in
         * timer's destructor (if the callback was started) you still need to make sure that
         * the owner of the callback function will not be deleted
         * until the timer is finished. It will be enough to call @ref stop with `true` as the first parameter
         * in the destructor of your type to wait for all running callback function threads to finish.
         *
         * @param iTimeToWaitInMs Time this timer should wait (in milliseconds) until the callback is called.
         * @param callback        Function to execute on timeout.
         * @param bIsLooping      Whether the timer should start again after a timeout or not.
         * If specified 'true', after the waiting time is over (timeout) the timer will automatically
         * restart itself and will start the waiting time again. There are 2 ways to stop a looping timer:
         * call @ref stop or destroy this object (destructor is called).
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
         *
         * @param bWaitForRunningCallbackThreadsToFinish Will be passed to @ref stop if the timer
         * is currently running (see @ref stop documentation on this parameter).
         */
        void start(bool bWaitForRunningCallbackThreadsToFinish);

        /**
         * Stops the timer and timer looping (if was specified in @ref start).
         *
         * @remark If a callback function was previously specified (see @ref setCallbackForTimeout),
         * the timer was running and the callback function was started it will continue running
         * without stopping. If the timer was running and the callback function was not started yet
         * it will be never started.
         *
         * @param bWaitForRunningCallbackThreadsToFinish If @ref setCallbackForTimeout was used, determines
         * whether we should wait for all currently running callback function threads to finish or not.
         * If @ref setCallbackForTimeout was not used, this value is ignored.
         * Here is an example describing what this parameter does:
         * you use @ref setCallbackForTimeout, then start the timer (@ref start) and the
         * callback function you specified was started and is currently running (in another thread)
         * but now you want the timer to stop, what should the timer do: should it wait for
         * this running callback function thread to finish (and block the current thread) or not?
         */
        void stop(bool bWaitForRunningCallbackThreadsToFinish);

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

        /** Waits for all threads from @ref mtxStartedCallbackThreads to finish and clears the array. */
        void waitForRunningCallbackThreadsToFinish();

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

        /** Array of started callback threads. */
        std::pair<std::mutex, std::vector<std::thread>> mtxStartedCallbackThreads;

        /** Condition variable for timer thread termination. */
        std::condition_variable cvTerminateTimerThread;

        /** Whether the destructor was called or not. */
        std::atomic_flag bIsShuttingDown{};

        /** Whether the timer was explicitly stopped or not. */
        std::atomic_flag bIsStopRequested{};

        /**
         * Whether the timer should log a warning message if waiting for started callback (in timer
         * destructor) is taking too long or not.
         */
        bool bWarnAboutWaitingForCallbackTooLong = true;

        /** Time to wait until the callback is called. */
        long long iTimeToWaitInMs = 0;

        /** Whether the timer should restart itself upon a timeout or not. */
        bool bIsLooping = false;
    };
} // namespace ne
