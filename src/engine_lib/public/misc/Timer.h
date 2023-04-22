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
        // Only node and game instance can create timers because they have some additional protection
        // code to avoid shooting yourself in the foot (like if you forget to stop the timer).
        // Although only nodes and game instances can create timers this does not mean that the timer
        // depends on their functionality - no, the timer is modular and you can use it outside of those
        // classes if you remove `friend class` clauses and move functions from `protected` section of
        // the timer to `public` section.
        friend class Node;
        friend class GameInstance;

    public:
        Timer() = delete;

        ~Timer();

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;

        /**
         * Sets a function to be executed when the waiting time is over (timeout event).
         *
         * Example:
         * @code
         * class GrenadeNode : public MeshNode
         * {
         * public:
         *     // ...
         *
         *     void throw(){
         *         pExplodeTimer->setCallbackForTimeout(3000, [this]() { explode(); });
         *         pExplodeTimer->start();
         *     }
         * private:
         *     void explode(){
         *         // ...
         *     }
         *
         *     Timer* pExplodeTimer = nullptr;
         * }
         * @endcode
         *
         * @remark If the timer is currently running (see @ref isRunning) this call will be ignored
         * and an error will be logged.
         *
         * @remark Upon a timeout the timer will submit a deferred task with your callback function
         * to the main thread because deferred tasks are executed each frame you might expect a slight
         * delay after the timeout event and before your callback is actually started, the delay
         * should be generally smaller that ~30 ms so it should not make a big difference to you,
         * but note that you probably want to avoid using callback timers for benchmarking or
         * other high precision timing events due to this delay.
         *
         * @param iTimeToWaitInMs Time this timer should wait (in milliseconds) until the callback is called.
         * @param callback        Function to execute on timeout.
         * @param bIsLooping      Whether the timer should start again after a timeout or not.
         * If specified `true`, after the waiting time is over (timeout) the timer will automatically
         * restart itself and will start the waiting time again.
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
         *
         * @remark If a callback function was previously specified (see @ref setCallbackForTimeout),
         * the timer was running and the callback function was started it will continue running
         * without stopping. If the timer was running and the callback function was not started yet
         * it will be never started.
         *
         * @param bDisableTimer Specify `true` to make future @ref start calls to be ignored,
         * `false` to allow restarting the timer.
         */
        void stop(bool bDisableTimer = false);

        /**
         * Returns the time that has passed since the timer was started (see @ref start).
         *
         * @return Empty if the @ref start was never called before,
         * otherwise time in milliseconds since the timer was started.
         *
         * @remark For looping timers (see @ref start) returns time since the beginning
         * of the current loop iteration. Each new loop will reset elapsed time to 0.
         *
         * @remark Note that if you call this function right after the call to @ref start
         * with a callback function set (see @ref setCallbackForTimeout)
         * this function may return empty because the timer thread is not started yet.
         */
        std::optional<long long> getElapsedTimeInMs();

        /**
         * Returns timer's name (only used for logging purposes).
         *
         * @return Timer's name.
         */
        std::string getName() const;

        /**
         * Returns the amount of times @ref start was called.
         *
         * @return @ref start call count.
         */
        size_t getStartCount();

        /**
         * Whether this timer is running (started) or not (finished/not started).
         *
         * @return `true` if currently running, `false` otherwise.
         */
        bool isRunning();

        /**
         * Whether this timer was running (started) and was stopped using @ref stop.
         *
         * @return `true` if the timer was stopped using @ref stop and is not running right now,
         * `false` otherwise.
         */
        bool isStopped();

        /**
         * Whether this timer can use @ref start function or not.
         *
         * @return `true` if @ref start can be called, `false` otherwise.
         */
        bool isEnabled();

    protected:
        /**
         * Constructor.
         *
         * @param sTimerName Name of this timer (used for logging). Don't add "timer" word to your timer's
         * name as it will be appended in the logs.
         */
        Timer(const std::string& sTimerName);

        /**
         * Sets a function to be called from a deferred task before the actual callback
         * to test if the actual callback should be started or not.
         *
         * @remark If the timer is currently running (see @ref isRunning) this call will be ignored
         * and an error will be logged.
         *
         * @param validator Validator function. The only parameter is @ref getStartCount at the moment
         * of timeout event. Returns `true` if the actual callback needs
         * to be started and `false` if the actual callback should not be started.
         */
        void setCallbackValidator(const std::function<bool(size_t)>& validator);

        /**
         * Determines whether @ref start will work or not.
         *
         * @param bEnable `true` to allow using @ref start, `false` otherwise.
         */
        void setEnable(bool bEnable);

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
         * Function to call from a deferred task before @ref callbackForTimeout to test if
         * the callback should be started or not.
         */
        std::optional<std::function<bool(size_t)>> callbackValidator;

        /** Name of this timer (used for logging). */
        std::string sTimerName;

        /**
         * Time when the @ref start was called.
         * Not empty if @ref start was called.
         */
        std::pair<std::mutex, std::optional<std::chrono::steady_clock::time_point>> mtxTimeWhenStarted;

        /** The number of times @ref start was called. */
        size_t iStartCount = 0;

        /** Mutex for read/write operations on data that the timer thread is using. */
        std::mutex mtxTerminateTimerThread;

        /** Condition variable for timer thread termination. */
        std::condition_variable cvTerminateTimerThread;

        /** Whether the destructor was called or not. */
        std::atomic_flag bIsShuttingDown{};

        /** Whether the timer was explicitly stopped or not. */
        std::atomic_flag bIsStopRequested{};

        /** Whether the timer is currently running or not. */
        bool bIsRunning = false;

        /**
         * `true` if @ref start calls should be allowed (able to restart the timer) or
         * `false` to ignore calls to @ref start function (won't be able to start the timer).
         */
        bool bIsEnabled = true;

        /** Time to wait until the callback is called. */
        long long iTimeToWaitInMs = 0;

        /** Whether the timer should restart itself upon a timeout or not. */
        bool bIsLooping = false;

        /** Name of the category used for logging. */
        static inline const auto sTimerLogCategory = "Timer";
    };
} // namespace ne
