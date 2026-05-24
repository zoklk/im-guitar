#pragma once

#include "ITechnicianState.h"

class TechnicianIdleState : public ITechnicianState {
public:
    static TechnicianIdleState& instance();

    void        update(Technician& t, int tick) override;
    const char* name() const override { return "Idle"; }

private:
    TechnicianIdleState() = default;
};

class TechnicianWorkingState : public ITechnicianState {
public:
    static TechnicianWorkingState& instance();

    void        onEnter(Technician& t, int tick) override;
    void        update(Technician& t, int tick) override;
    const char* name() const override { return "Working"; }

private:
    TechnicianWorkingState() = default;
};
