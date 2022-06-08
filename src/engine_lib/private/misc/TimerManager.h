#pragma once

// STL.
#include <vector>
#include <memory>
#include <mutex>

// Custom.
#include "misc/Timer.h"

namespace ne {
    /** Controls timers. */
    class TimerManager {
    public:
        TimerManager(const TimerManager&) = delete;
        TimerManager& operator=(const TimerManager&) = delete;

        /**
         * Creates a new timer.
         *
         * @return New timer.
         */
        std::shared_ptr<Timer> createTimer();

        /** Destructor. */
        ~TimerManager();

    private:
        friend class Game;

        /** Constructor. */
        TimerManager() = default;

        /** Array of created timers. */
        std::pair<std::mutex, std::vector<std::shared_ptr<Timer>>> mtxCreatedTimers;
    };
} // namespace ne
