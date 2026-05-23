#pragma once

#include "IMachineState.h"

class MachineIdleState : public IMachineState {
public:
    static MachineIdleState& instance();

    void        update(Machine& m, int tick) override;
    const char* name() const override { return "Idle"; }

private:
    MachineIdleState() = default;
};

class MachineProcessingState : public IMachineState {
public:
    static MachineProcessingState& instance();

    void        onEnter(Machine& m, int tick) override;
    void        update(Machine& m, int tick) override;
    const char* name() const override { return "Processing"; }

private:
    MachineProcessingState() = default;
};

class MachineBrokenState : public IMachineState {
public:
    static MachineBrokenState& instance();

    void        onEnter(Machine& m, int tick) override;
    void        update(Machine& m, int tick) override;
    const char* name() const override { return "Broken"; }

private:
    MachineBrokenState() = default;
};
