#include "TechnicianStates.h"

#include "Technician.h"
#include "model/machine/Machine.h"

// ── TechnicianIdleState ─────────────────────────────────────────
TechnicianIdleState& TechnicianIdleState::instance() {
    static TechnicianIdleState inst;
    return inst;
}

void TechnicianIdleState::update(Technician& /*t*/, int /*tick*/) {
    // no-op — 외부 assign() 대기
}

// ── TechnicianWorkingState ──────────────────────────────────────
TechnicianWorkingState& TechnicianWorkingState::instance() {
    static TechnicianWorkingState inst;
    return inst;
}

void TechnicianWorkingState::onEnter(Technician& t, int /*tick*/) {
    t.repairProgress_ = 0;
}

void TechnicianWorkingState::update(Technician& t, int tick) {
    ++t.repairProgress_;
    if (t.repairProgress_ >= t.repairTime_) {
        t.targetMachine_->repair(tick);
        t.targetMachine_ = nullptr;
        t.transitionTo(TechnicianIdleState::instance(), tick);
    }
}
