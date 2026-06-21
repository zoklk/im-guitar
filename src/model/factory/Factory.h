#pragma once

#include <memory>
#include <random>
#include <string>
#include <vector>

#include "common/FactorySnap.h"
#include "common/Types.h"
#include "model/machine/IMachineLookup.h"
#include "model/product/ProductIdGen.h"

class EventBroker;
class EventLog;
class Statistics;
class RepairDispatcher;
class Machine;
class Conveyor;
class Technician;
struct ScenarioConfig;
struct MachineDef;
struct ConveyorDef;
struct TechnicianDef;

// 객체 소유 컨테이너 + 외부 cmd 수신 + snap 생성기.
// 클럭은 SimulationRunner가 담당.
class Factory : public IMachineLookup {
public:
    Factory(EventBroker&       broker,
            EventLog&          eventLog,
            Statistics&        statistics,
            RepairDispatcher&  repairDispatcher);

    // unique_ptr<forward-declared> 멤버 → 소멸자를 .cpp에 정의해 완전 타입 의존을 격리.
    ~Factory();

    // ── 외부 cmd 메서드 (Controller가 호출) ─────────────────
    void reset();                                     // 모든 객체 / 카운터 클리어, broker 토픽 구독 정리
    void applyConfig(const ScenarioConfig& cfg);      // create들 + 와이어링 + priorityMap + Backpressure/Cascade 토픽
    void setScenario(ScenarioType s) { scenario_ = s; }
    void forceBreak(const std::string& machineId);
    void instantRepair(const std::string& machineId);
    void clearLog();

    // ── 시뮬레이션 ──────────────────────────────────────────
    void        tick();
    FactorySnap snapshot() const;
    void        restore(const FactorySnap& snap);

    // ── 조회 ────────────────────────────────────────────────
    Machine*    findMachine(const std::string& id) override;
    Conveyor*   findConveyor(const std::string& id);
    Technician* findTechnician(const std::string& id);

    // ── getter ──────────────────────────────────────────────
    int          getTick() const     { return tick_; }
    ScenarioType getScenario() const { return scenario_; }

    const std::vector<std::unique_ptr<Machine>>&    getMachines()    const { return machines_; }
    const std::vector<std::unique_ptr<Conveyor>>&   getConveyors()   const { return conveyors_; }
    const std::vector<std::unique_ptr<Technician>>& getTechnicians() const { return technicians_; }

private:
    // ── 생성 (Factory 패턴, 타입별 switch 1곳) ──────────────
    Machine*    createMachine(const MachineDef& def, OverflowMode mode);
    Conveyor*   createConveyor(const ConveyorDef& def);
    Technician* createTechnician(const TechnicianDef& def);

    // ── 와이어링 보조 ───────────────────────────────────────
    void wireConveyorsToMachines(const ScenarioConfig& cfg);
    void computeAndInjectPriorityMap(const ScenarioConfig& cfg);
    void subscribeBackpressureTopics(const ScenarioConfig& cfg);   // SmartFactory 한정
    void subscribeFaultCascadeTopics(const ScenarioConfig& cfg);   // SmartFactory 한정

    // 협력자
    EventBroker&       broker_;
    EventLog&          eventLog_;
    Statistics&        statistics_;
    RepairDispatcher&  repairDispatcher_;

    // 소유 컨테이너
    std::vector<std::unique_ptr<Machine>>    machines_;
    std::vector<std::unique_ptr<Conveyor>>   conveyors_;
    std::vector<std::unique_ptr<Technician>> technicians_;

    // 시뮬 상태
    int          tick_     = 0;
    ScenarioType scenario_ = ScenarioType::Normal;
    std::mt19937 rng_;
    ProductIdGen idGen_;
};
