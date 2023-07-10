#pragma once

// Standard.
#include <atomic>
#include <functional>
#include <mutex>
#include <queue>
#include <condition_variable>

namespace ne {
    /**
     * A very simple thread pool.
     */
    class ThreadPool {
    public:
        /** Creates threads to execute tasks. */
        ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        /** Waits for all threads to stop. */
        ~ThreadPool();

        /**
         * Adds a new task to be executed in the thread pool.
         *
         * @param task A task to add.
         */
        void addTask(const std::function<void()>& task);

        /**
         * Stop all working threads.
         * Can be called explicitly. If not called explicitly will
         * be called in destructor.
         */
        void stop();

    protected:
        /**
         * Function that each thread is executing.
         * Waits for new tasks and processes one.
         */
        void processTasksThread();

    private:
        /** Condition variable to wait until new tasks are added. */
        std::condition_variable cvNewTasks;

        /** Array of running threads. */
        std::vector<std::thread> vRunningThreads;

        /**
         * Mutex for read/write operations on task queue.
         * Task queue that contains all tasks to be executed in the thread pool.
         */
        std::pair<std::mutex, std::queue<std::function<void()>>> mtxTaskQueue;

        /**
         * Atomic flag to set when destructor is called so that running threads
         * are notified to finish.
         */
        std::atomic_flag bIsShuttingDown;

        /** Minimum amount of threads to create when hardware concurrency information is not available. */
        const unsigned int iMinThreadCount = 4;
    };
} // namespace ne
