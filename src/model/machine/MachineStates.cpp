#include "MachineStates.h"

#include <random>

#include "Machine.h"

// ── MachineIdleState ────────────────────────────────────────────
MachineIdleState& MachineIdleState::instance() {
    static MachineIdleState inst;
    return inst;
}

void MachineIdleState::update(Machine& m, int tick) {
    // forceBreak로 health=0인 채 Idle에 머무는 경우 즉시 Broken 전이
    if (m.health_ <= 0) {
        m.transitionTo(MachineBrokenState::instance(), tick);
        return;
    }
    if (m.canStart()) {
        m.transitionTo(MachineProcessingState::instance(), tick);
    }
}

// ── MachineProcessingState ──────────────────────────────────────
MachineProcessingState& MachineProcessingState::instance() {
    static MachineProcessingState inst;
    return inst;
}

void MachineProcessingState::onEnter(Machine& m, int tick) {
    // 비어있지 않으면 Broken→repair 복귀 (resume) → 보존된 상태 그대로 진행
    if (m.currentProduct_.empty()) {
        m.processingTick_ = 0;
        m.gatherInputs();
        m.publishStarted(tick);
    }
}

void MachineProcessingState::update(Machine& m, int tick) {
    if (m.breakdownProb_ > 0.0) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        if (dist(m.rng_) < m.breakdownProb_) {
            m.health_--;
            if (m.health_ <= 0) {
                m.health_ = 0;
                m.transitionTo(MachineBrokenState::instance(), tick);
                return;   // currentProduct / processingTick 보존 (repair 시 이어서 처리)
            }
        }
    }

    m.processingTick_++;
    if (m.processingTick_ >= m.processingTime_) {
        m.process(tick);
        m.processingTick_ = 0;
        m.transitionTo(MachineIdleState::instance(), tick);
    }
}

// ── MachineBrokenState ──────────────────────────────────────────
MachineBrokenState& MachineBrokenState::instance() {
    static MachineBrokenState inst;
    return inst;
}

void MachineBrokenState::onEnter(Machine& m, int tick) {
    m.publishEvent(EventType::Fault, tick);
}

void MachineBrokenState::update(Machine& /*m*/, int /*tick*/) {
    // no-op — 외부 repair() 대기
}
