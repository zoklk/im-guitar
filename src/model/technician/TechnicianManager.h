#pragma once

#include <string>
#include <vector>

#include "common/Event.h"

class EventBroker;
class IMachineLookup;
class Machine;
class Technician;

// Observer 패턴: Fault 이벤트를 구독해 수리 대기 큐를 관리하고
// idle Technician에 배정. 큐 정렬 정책은 priority(↑) → faultTick(↑) → seq(↑).
//
// priority: Packager 의존성 그래프 역방향 거리 기반 정적 테이블 (priority 값이
// 낮을수록 먼저 수리). 명세 phase_5_orchestrator.md 표 참고.
//
// 머신 lookup은 IMachineLookup 인터페이스에 의존 (Factory는 Phase 6 구현).
// Phase 5/6 의존성 역전 — TechnicianManager는 Factory 헤더를 include하지 않음.
//
// update(tick) 동작:
//   1) 큐 상단이 더 이상 broken이 아니면 pop (instantRepair 우회 대응)
//   2) 큐 정렬
//   3) idle technician 발견 시 큐 앞에서 pop → tech.assign(machine, tick)
//
// 1틱 지연: EventBroker는 publish 큐 적재 후 flush 시 dispatch. 같은 틱에
// publish된 Fault는 다음 틱 update에서 처리됨 (broker.flush가 update보다
// 먼저 호출되는 Phase 6 Factory.tick 순서에 의존).
class TechnicianManager : public IEventHandler {
public:
    struct QueueEntry {
        Machine* machine   = nullptr;
        int      priority  = 0;
        int      faultTick = 0;
        int      seq       = 0;
    };

    TechnicianManager(EventBroker&    broker,
                      IMachineLookup& lookup);

    // 외부 와이어링 (Factory가 Technician 생성 시 등록).
    void registerTechnician(Technician* t);

    // IEventHandler
    void handle(const Event& ev) override;

    // 매 틱 Factory.tick 마지막에 호출.
    void update(int tick);

    // 메멘토 복원 / 초기화.
    void clearQueue();
    void restoreQueue(const std::vector<QueueEntry>& entries, int nextSeq);

    // Read-only 접근자 (Factory.snapshot / 테스트).
    const std::vector<QueueEntry>& getQueue() const { return repairQueue_; }
    int                            getNextSeq() const { return nextSeq_; }
    const std::vector<Technician*>& getTechnicians() const { return technicians_; }

    // 정적 priority 테이블 외부 노출 (Factory.snapshot에서 RepairOrderSnap 작성 시
    // priority가 이미 큐에 저장되어 있으나, 테스트 검증용으로 노출).
    static int priorityOf(int machineTypeEnumValue);

private:
    EventBroker&             broker_;
    IMachineLookup&          lookup_;
    std::vector<Technician*> technicians_;
    std::vector<QueueEntry>  repairQueue_;
    int                      nextSeq_ = 0;
};
