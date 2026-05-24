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
    // 호출 전제: Idle 상태에서만 호출. Working 중 재배정은 호출자 버그.
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
