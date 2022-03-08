#pragma once

#include "IGameInstance.h"

class EditorGameInstance : public ne::IGameInstance {
public:
    EditorGameInstance() = default;
    virtual ~EditorGameInstance() override = default;

    virtual void onBeforeNewFrame(float fTimeFromPrevCallInSec) override{};
};
