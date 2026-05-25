#include "Technician.h"

#include <cassert>
#include <utility>

#include "TechnicianStates.h"

Technician::Technician(std::string  id,
                       int          repairTime,
                       EventBroker& broker)
    : SimulationObject(broker),
      id_(std::move(id)),
      repairTime_(repairTime) {
    currentState_ = &TechnicianIdleState::instance();
}

void Technician::update(int tick) {
    currentState_->update(*this, tick);
}

void Technician::assign(Machine* m, int tick) {
    assert(currentState_ == &TechnicianIdleState::instance());
    assert(m != nullptr);
    targetMachine_ = m;
    transitionTo(TechnicianWorkingState::instance(), tick);
}

void Technician::transitionTo(ITechnicianState& next, int tick) {
    if (currentState_) currentState_->onExit(*this, tick);
    currentState_ = &next;
    currentState_->onEnter(*this, tick);
}

bool Technician::isIdle() const {
    return currentState_ == &TechnicianIdleState::instance();
}
