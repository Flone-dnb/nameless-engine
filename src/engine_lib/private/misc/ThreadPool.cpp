﻿#include "ThreadPool.h"

// Standard.
#include <thread>

namespace ne {
    ThreadPool::ThreadPool() {
        const auto iThreadCount = std::thread::hardware_concurrency();
        vRunningThreads.resize(iThreadCount);
        for (unsigned int i = 0; i < iThreadCount; i++) {
            vRunningThreads[i] = std::thread(&ThreadPool::processTasksThread, this);
        }
    }

    void ThreadPool::processTasksThread() {
        do {
            std::function<void()> task;
            {
                std::unique_lock guard(mtxTaskQueue.first);
                cvNewTasks.wait(guard, [this] {
                    return !mtxTaskQueue.second.empty() || bIsShuttingDown.test(std::memory_order_seq_cst);
                });
                if (bIsShuttingDown.test(std::memory_order_seq_cst)) {
                    return;
                }

                task = mtxTaskQueue.second.front();
                mtxTaskQueue.second.pop();
            }
            task();
        } while (!bIsShuttingDown.test());
    }

    void ThreadPool::addTask(const std::function<void()>& task) {
        std::scoped_lock guard(mtxTaskQueue.first);
        mtxTaskQueue.second.push(task);

        cvNewTasks.notify_one();
    }

    void ThreadPool::stop() {
        if (bIsShuttingDown.test()) {
            return;
        }

        bIsShuttingDown.test_and_set(std::memory_order_seq_cst);

        cvNewTasks.notify_all();

        for (auto& thread : vRunningThreads) {
            thread.join();
        }

        vRunningThreads.clear();
    }

    ThreadPool::~ThreadPool() { stop(); }

} // namespace ne
