#pragma once

#include <string>

#include "model/SimulationObject.h"

class EventBroker;
class ITechnicianState;
class Machine;

// 수리공 객체. SimulationObject 직접 상속.
// State 패턴 (TechnicianIdleState / TechnicianWorkingState 싱글톤 + DI).
//
// 책임:
//   - 한 번에 한 머신만 수리 (targetMachine_)
//   - repairTime_ 만큼 진행 후 machine.repair() 호출 → Idle 복귀
//   - 큐 관리는 TechnicianManager 책임 (Phase 5)
//
// targetMachine_ 불변: Working 상태에서만 non-null. 진입점은 단 두 곳
//   1) assign(m, tick): nullptr → m, IdleState → WorkingState
//   2) WorkingState.update 완료 분기: m → nullptr, WorkingState → IdleState
// 두 곳 외에서 targetMachine_을 건드리지 않음 → assert 불필요.
//
// Resume 이벤트 발행은 Machine.repair() 내부에서 sourceId=machine.id로.
// Technician은 자체 이벤트 발행 없음 (instantRepair 경로와 일관성).
class Technician : public SimulationObject {
public:
    Technician(std::string  id,
               int          repairTime,
               EventBroker& broker);

    // SimulationObject
    void               update(int tick) override;
    const std::string& getId() const override { return id_; }

    // 외부 명령 (TechnicianManager가 호출)
    // 호출 전제: 현재 Idle 상태. Working 중 재배정은 호출자 측 버그로 간주 → assert.
    void assign(Machine* m, int tick);

    // State 전이 (TechnicianWorkingState가 완료 시 호출)
    void transitionTo(ITechnicianState& next, int tick);

    // 조회 (TechnicianManager / Factory.snapshot / 테스트)
    bool                 isIdle() const;
    Machine*             getTargetMachine() const   { return targetMachine_; }
    int                  getRepairTime() const      { return repairTime_; }
    int                  getRepairProgress() const  { return repairProgress_; }
    const ITechnicianState* getCurrentState() const { return currentState_; }

private:
    friend class TechnicianIdleState;
    friend class TechnicianWorkingState;

    std::string       id_;
    int               repairTime_;
    int               repairProgress_ = 0;
    Machine*          targetMachine_  = nullptr;
    ITechnicianState* currentState_   = nullptr;
};
