#include "misc/TimerManager.h"

// Custom.
#include "io/Logger.h"

namespace ne {
    std::shared_ptr<Timer> TimerManager::createTimer() {
        std::scoped_lock guard(mtxCreatedTimers.first);

        std::shared_ptr<Timer> pNewTimer = std::shared_ptr<Timer>(new Timer(this));
        bool bReplacedOldTimer = false;

        // See if we can replace (delete) unused timer.
        for (size_t i = 0; i < mtxCreatedTimers.second.size(); i++) { // NOLINT
            if (mtxCreatedTimers.second[i].use_count() == 1) {
                mtxCreatedTimers.second[i] = pNewTimer;
                bReplacedOldTimer = true;
                break;
            }
        }

        if (!bReplacedOldTimer) {
            mtxCreatedTimers.second.push_back(pNewTimer);
        }

        return pNewTimer;
    }

    TimerManager::~TimerManager() {
        std::scoped_lock guard(mtxCreatedTimers.first);

        for (const auto& pTimer : mtxCreatedTimers.second) {
            auto iUseCount = pTimer.use_count();
            if (iUseCount != 1) {
                Logger::get().error(
                    std::format(
                        "timer {} is still referenced somewhere else and will not be deleted (use count: {}) "
                        "(did you forgot to call Timer::stop() after the timer is not needed anymore?)",
                        static_cast<void*>(pTimer.get()),
                        iUseCount),
                    "");
            }
        }
    }
} // namespace ne
