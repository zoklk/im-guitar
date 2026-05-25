#pragma once

class Machine;

class IMachineState {
public:
    virtual ~IMachineState() = default;

    virtual void        update(Machine& m, int tick)   = 0;
    virtual void        onEnter(Machine& /*m*/, int /*tick*/) {}
    virtual void        onExit(Machine& /*m*/, int /*tick*/)  {}
    virtual const char* name() const                   = 0;
};
