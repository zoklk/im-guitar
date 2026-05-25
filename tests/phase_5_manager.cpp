// Phase 5 — TechnicianManager 단위 테스트.
// Fault 구독 → 큐 진입, 우선순위 정렬, FIFO 동률, idle technician 배정,
// instantRepair 우회 시 stale 큐 항목 자동 pop 검증.

#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>

#include "common/Event.h"
#include "common/Types.h"
#include "model/event/EventBroker.h"
#include "model/machine/IMachineLookup.h"
#include "model/machine/Machine.h"
#include "model/machine/MachineStates.h"
#include "model/product/ProductIdGen.h"
#include "model/technician/Technician.h"
#include "model/technician_manager/TechnicianManager.h"

#include <gtest/gtest.h>

namespace {

// Phase 4 테스트와 동일한 SpyMachine 패턴.
class SpyMachine : public Machine {
public:
    SpyMachine(std::string id, MachineType type, EventBroker& broker,
               std::mt19937& rng, ProductIdGen& idGen)
        : Machine(std::move(id), type,
                  /*processingTime=*/1, /*bp=*/0.0, /*requiredCount=*/1,
                  OverflowMode::Drop, broker, rng, idGen) {}

    void process(int /*tick*/) override {}
};

// 테스트용 IMachineLookup 구현.
class StubLookup : public IMachineLookup {
public:
    void add(Machine* m) { map_[m->getId()] = m; }
    Machine* findMachine(const std::string& id) override {
        auto it = map_.find(id);
        return it == map_.end() ? nullptr : it->second;
    }
private:
    std::unordered_map<std::string, Machine*> map_;
};

std::mt19937 makeRng(uint32_t seed = 7) { return std::mt19937(seed); }
ProductIdGen& sharedIdGen() {
    static ProductIdGen g;
    return g;
}

// 머신을 BrokenState로 진입시키며 Fault 이벤트를 자동 발행 → broker.flush로
// TechnicianManager.handle까지 한 번에 도달. forceBreak() 만으로는 health_=0만
// 설정되고 currentState_ 전이는 일어나지 않으므로 update를 한 번 호출.
void breakAndDispatchFault(Machine& m, EventBroker& broker, int tick) {
    m.forceBreak();
    m.update(tick);   // IdleState::update가 health<=0 감지 → BrokenState 전이 → Fault publish
    broker.flush();
}

}  // namespace

// ─────────────────────────────────────────────────────────────
// 1. Fault 이벤트 수신 → 큐 진입
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechManager, FaultEnqueuesEntry) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m("M_PACK", MachineType::Packager, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&m);
    TechnicianManager mgr(broker, lookup);

    breakAndDispatchFault(m, broker, /*tick=*/10);

    ASSERT_EQ(mgr.getQueue().size(), 1u);
    EXPECT_EQ(mgr.getQueue()[0].machine, &m);
    EXPECT_EQ(mgr.getQueue()[0].priority, 0);   // Packager
    EXPECT_EQ(mgr.getQueue()[0].faultTick, 10);
    EXPECT_EQ(mgr.getQueue()[0].seq, 0);
}

TEST(PhaseTechManager, UnknownSourceIdIgnored) {
    EventBroker broker;
    StubLookup lookup;
    TechnicianManager mgr(broker, lookup);

    // 등록 안 된 sourceId로 Fault 직접 발행
    Event ev;
    ev.type     = EventType::Fault;
    ev.sourceId = "NOPE";
    ev.tick     = 5;
    broker.publish(ev);
    broker.flush();
    EXPECT_TRUE(mgr.getQueue().empty());
}

// ─────────────────────────────────────────────────────────────
// 2. 우선순위 정렬: Packager + HeadCutter → Packager 먼저
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechManager, PriorityOrderingPackagerBeforeHeadCutter) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine pack("M_PACK", MachineType::Packager,  broker, rng, sharedIdGen());
    SpyMachine head("M_HEAD", MachineType::HeadCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&pack);
    lookup.add(&head);
    TechnicianManager mgr(broker, lookup);

    // HeadCutter 먼저 발행해도 Packager가 더 높은 우선순위(priority=0)
    breakAndDispatchFault(head, broker, 10);
    breakAndDispatchFault(pack, broker, 10);

    Technician t("T1", /*repairTime=*/2, broker);
    mgr.registerTechnician(&t);

    mgr.update(11);
    // Packager가 먼저 assign되어야 함
    EXPECT_EQ(t.getTargetMachine(), &pack);
}

// ─────────────────────────────────────────────────────────────
// 3. FIFO 동률: HeadCutter / NeckCutter (priority 동일 3) 동시 fault
//    → seq 작은 쪽 먼저
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechManager, FifoTieBreakBySeq) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine head("M_HEAD", MachineType::HeadCutter, broker, rng, sharedIdGen());
    SpyMachine neck("M_NECK", MachineType::NeckCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&head);
    lookup.add(&neck);
    TechnicianManager mgr(broker, lookup);

    // 같은 tick에 head → neck 순으로 발행
    breakAndDispatchFault(head, broker, 10);
    breakAndDispatchFault(neck, broker, 10);

    Technician t("T1", 2, broker);
    mgr.registerTechnician(&t);

    mgr.update(11);
    EXPECT_EQ(t.getTargetMachine(), &head);   // 먼저 큐 진입한 head
}

// ─────────────────────────────────────────────────────────────
// 4. idle Technician에 assign — 큐에서 pop
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechManager, AssignsToIdleTechnicianAndPopsQueue) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m1("M1", MachineType::HeadCutter, broker, rng, sharedIdGen());
    SpyMachine m2("M2", MachineType::NeckCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&m1);
    lookup.add(&m2);
    TechnicianManager mgr(broker, lookup);

    breakAndDispatchFault(m1, broker, 10);
    breakAndDispatchFault(m2, broker, 10);

    Technician t1("T1", 2, broker);
    Technician t2("T2", 2, broker);
    mgr.registerTechnician(&t1);
    mgr.registerTechnician(&t2);

    mgr.update(11);

    EXPECT_TRUE(mgr.getQueue().empty());
    EXPECT_FALSE(t1.isIdle());
    EXPECT_FALSE(t2.isIdle());
}

TEST(PhaseTechManager, BusyTechnicianNotReassigned) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m1("M1", MachineType::HeadCutter, broker, rng, sharedIdGen());
    SpyMachine m2("M2", MachineType::NeckCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&m1);
    lookup.add(&m2);
    TechnicianManager mgr(broker, lookup);

    Technician t1("T1", /*repairTime=*/5, broker);
    mgr.registerTechnician(&t1);

    breakAndDispatchFault(m1, broker, 1);
    mgr.update(2);
    ASSERT_FALSE(t1.isIdle());

    // 두 번째 Fault 발생 — 큐에 들어가나 t1은 busy라 배정 못 함
    breakAndDispatchFault(m2, broker, 3);
    mgr.update(4);

    ASSERT_EQ(mgr.getQueue().size(), 1u);
    EXPECT_EQ(mgr.getQueue()[0].machine, &m2);
    EXPECT_EQ(t1.getTargetMachine(), &m1);   // 여전히 m1 수리 중
}

// ─────────────────────────────────────────────────────────────
// 5. instantRepair 우회: 큐 상단 머신이 이미 idle이면 pop
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechManager, StaleQueueHeadIsPoppedAutomatically) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m1("M1", MachineType::HeadCutter, broker, rng, sharedIdGen());
    SpyMachine m2("M2", MachineType::NeckCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&m1);
    lookup.add(&m2);
    TechnicianManager mgr(broker, lookup);

    breakAndDispatchFault(m1, broker, 1);
    breakAndDispatchFault(m2, broker, 2);
    ASSERT_EQ(mgr.getQueue().size(), 2u);

    // 외부에서 instantRepair 우회 → m1.repair()로 broken 해제
    m1.repair(3);
    broker.flush();   // repair가 발행한 Resume 이벤트 처리 (Manager에는 영향 없음)
    ASSERT_NE(m1.getCurrentState(), &MachineBrokenState::instance());

    Technician t1("T1", 2, broker);
    mgr.registerTechnician(&t1);

    mgr.update(4);
    // m1은 stale로 pop, m2가 t1에 배정
    EXPECT_EQ(t1.getTargetMachine(), &m2);
    EXPECT_TRUE(mgr.getQueue().empty());
}

// ─────────────────────────────────────────────────────────────
// 6. priorityOf 정적 테이블 검증
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechManager, PriorityTableMatchesSpec) {
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::Packager)),          0);
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::PartAssembler)),     1);
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::BodyAssembler)),     2);
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::ElecPartCollector)), 2);
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::HeadCutter)),        3);
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::NeckCutter)),        3);
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::Painter)),           3);
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::BridgeSpawner)),     3);
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::PickupSpawner)),     3);
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::BodyCutter)),        4);
    EXPECT_EQ(TechnicianManager::priorityOf(static_cast<int>(MachineType::WoodSpawner)),       4);
}

// ─────────────────────────────────────────────────────────────
// 7. clearQueue / restoreQueue (메멘토 복원 시뮬레이션)
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechManager, ClearAndRestoreQueue) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m("M1", MachineType::HeadCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&m);
    TechnicianManager mgr(broker, lookup);

    breakAndDispatchFault(m, broker, 1);
    ASSERT_EQ(mgr.getQueue().size(), 1u);

    mgr.clearQueue();
    EXPECT_TRUE(mgr.getQueue().empty());
    EXPECT_EQ(mgr.getNextSeq(), 0);

    std::vector<TechnicianManager::QueueEntry> restored = {
        {&m, /*priority=*/3, /*faultTick=*/5, /*seq=*/42},
    };
    mgr.restoreQueue(restored, /*nextSeq=*/43);
    ASSERT_EQ(mgr.getQueue().size(), 1u);
    EXPECT_EQ(mgr.getQueue()[0].seq, 42);
    EXPECT_EQ(mgr.getNextSeq(), 43);
}
