#pragma once

namespace dxe {
    /**
     * Reacts to user inputs, window events and etc.
     */
    class IGameInstance {
    public:
        IGameInstance() = default;

        IGameInstance(const IGameInstance &) = delete;
        IGameInstance &operator=(const IGameInstance &) = delete;

        virtual ~IGameInstance() = default;

        virtual void onBeforeNewFrame(float fTimeFromPrevCallInSec) = 0;
    };

} // namespace dxe