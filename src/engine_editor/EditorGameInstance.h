#pragma once

#include "IGameInstance.h"

class EditorGameInstance : public dxe::IGameInstance {
public:
    EditorGameInstance() = default;
    virtual ~EditorGameInstance() override = default;

    virtual void onBeforeNewFrame(float fTimeFromPrevCallInSec) override{};
};
