#pragma once

#include "IGameInstance.h"

/**
 * Editor logic.
 */
class EditorGameInstance : public ne::IGameInstance {
public:
    EditorGameInstance() = default;
    virtual ~EditorGameInstance() override = default;

    virtual void onBeforeNewFrame(float fTimeFromPrevCallInSec) override{};
};
