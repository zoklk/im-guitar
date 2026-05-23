#pragma once
#include "common/FactorySnap.h"

class Controller; 

class Panel {
public:
    virtual ~Panel() = default;

    virtual void render(const FactorySnap& snap, Controller* ctrl) = 0;
};