#pragma once
#include "../common/FactorySnap.h"
#include "../common/MachineCmd.h"

class Panel {
public:
    virtual ~Panel() = default;
    // Controller* 대신 MachineCmd& 로 변경!
    virtual void render(const FactorySnap& snap, MachineCmd& cmd) = 0;
};