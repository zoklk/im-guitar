#pragma once

class Technician;

class ITechnicianState {
public:
    virtual ~ITechnicianState() = default;

    virtual void        update(Technician& t, int tick) = 0;
    virtual void        onEnter(Technician& /*t*/, int /*tick*/) {}
    virtual void        onExit(Technician& /*t*/, int /*tick*/)  {}
    virtual const char* name() const                    = 0;
};
