#pragma once

namespace ne {
    /**
     * Reacts to user inputs, window events and etc.
     */
    class IGameInstance {
    public:
        IGameInstance() = default;

        IGameInstance(const IGameInstance &) = delete;
        IGameInstance &operator=(const IGameInstance &) = delete;

        virtual ~IGameInstance() = default;

        /**
         * Called before a frame is rendered.
         *
         * @param fTimeFromPrevCallInSec   Time in seconds that has passed since the last call
         * to this function.
         */
        virtual void onBeforeNewFrame(float fTimeFromPrevCallInSec) = 0;

        /**
         * Called when the window was requested to close.
         */
        virtual void onWindowClose() = 0;

        /**
         * Returns the time in seconds that has passed
         * since the very first window was created.
         */
        static float getTotalApplicationTimeInSec();
    };

} // namespace ne