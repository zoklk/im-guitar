#pragma once

#include "common/FactorySnap.h"
#include "common/MachineCmd.h"

class Panel {
public:
    virtual ~Panel() = default;

    virtual void render(const FactorySnap& snap, MachineCmd& out) = 0;
};