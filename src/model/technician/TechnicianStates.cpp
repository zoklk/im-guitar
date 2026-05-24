#include "TechnicianStates.h"

#include "Technician.h"
#include "model/machine/Machine.h"

// ── TechnicianIdleState ─────────────────────────────────────────
TechnicianIdleState& TechnicianIdleState::instance() {
    static TechnicianIdleState inst;
    return inst;
}

void TechnicianIdleState::update(Technician& /*t*/, int /*tick*/) {
    // no-op — TechnicianManager가 assign 호출 시 외부에서 Working으로 전이
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
        // 완료 시점 tick으로 repair 호출 (Machine이 Resume publish 시 사용)
        t.targetMachine_->repair(tick);
        t.targetMachine_ = nullptr;
        t.transitionTo(TechnicianIdleState::instance(), tick);
    }
}
