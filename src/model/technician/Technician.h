#pragma once

#include <string>

#include "model/SimulationObject.h"

class EventBroker;
class ITechnicianState;
class Machine;

// targetMachine_ 불변 (Working 상태에서만 non-null):
//   - 셋: assign(m, tick) 한 곳에서만 (nullptr → m)
//   - 리셋: WorkingState.update 완료 분기 한 곳에서만 (m → nullptr)
class Technician : public SimulationObject {
public:
    Technician(std::string  id,
               int          repairTime,
               EventBroker& broker);

    void               update(int tick) override;
    const std::string& getId() const override { return id_; }

    // 호출 전제: 현재 Idle. Working 중 재배정은 호출자 버그.
    void assign(Machine* m, int tick);

    void transitionTo(ITechnicianState& next, int tick);

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
