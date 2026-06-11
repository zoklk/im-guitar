// Phase 5 — RepairDispatcher 단위 테스트.
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
#include "model/repair_dispatcher/RepairDispatcher.h"

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
// RepairDispatcher.handle까지 한 번에 도달.
void breakAndDispatchFault(Machine& m, EventBroker& broker, int tick) {
    m.forceBreak();
    m.update(tick);   // IdleState::update가 health<=0 감지 → BrokenState 전이 → Fault publish
    broker.flush();
}

}  // namespace

// ─────────────────────────────────────────────────────────────
// 1. Fault 이벤트 수신 → 큐 진입
// ─────────────────────────────────────────────────────────────

TEST(PhaseRepairDispatcher, FaultEnqueuesEntry) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m("M_PACK", MachineType::Packager, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&m);
    RepairDispatcher dispatcher(broker, lookup);
    dispatcher.setPriorityMap({{"M_PACK", 0}});

    breakAndDispatchFault(m, broker, /*tick=*/10);

    ASSERT_EQ(dispatcher.getQueue().size(), 1u);
    EXPECT_EQ(dispatcher.getQueue()[0].machine, &m);
    EXPECT_EQ(dispatcher.getQueue()[0].priority, 0);
    EXPECT_EQ(dispatcher.getQueue()[0].faultTick, 10);
    EXPECT_EQ(dispatcher.getQueue()[0].seq, 0);
}

TEST(PhaseRepairDispatcher, UnknownSourceIdIgnored) {
    EventBroker broker;
    StubLookup lookup;
    RepairDispatcher dispatcher(broker, lookup);

    // 등록 안 된 sourceId로 Fault 직접 발행
    Event ev;
    ev.type     = EventType::Fault;
    ev.sourceId = "NOPE";
    ev.tick     = 5;
    broker.publish(ev);
    broker.flush();
    EXPECT_TRUE(dispatcher.getQueue().empty());
}

TEST(PhaseRepairDispatcher, UnknownPriorityFallsBackTo99) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m("M_X", MachineType::HeadCutter, broker, rng, sharedIdGen());
    StubLookup lookup;
    lookup.add(&m);
    RepairDispatcher dispatcher(broker, lookup);
    // setPriorityMap 호출 없음 → priorityOf("M_X")는 99 fallback
    breakAndDispatchFault(m, broker, 1);
    ASSERT_EQ(dispatcher.getQueue().size(), 1u);
    EXPECT_EQ(dispatcher.getQueue()[0].priority, 99);
}

// ─────────────────────────────────────────────────────────────
// 2. 우선순위 정렬: Packager + HeadCutter → Packager 먼저
// ─────────────────────────────────────────────────────────────

TEST(PhaseRepairDispatcher, PriorityOrderingPackagerBeforeHeadCutter) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine pack("M_PACK", MachineType::Packager,  broker, rng, sharedIdGen());
    SpyMachine head("M_HEAD", MachineType::HeadCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&pack);
    lookup.add(&head);
    RepairDispatcher dispatcher(broker, lookup);
    dispatcher.setPriorityMap({{"M_PACK", 0}, {"M_HEAD", 3}});

    // HeadCutter 먼저 발행해도 Packager가 더 높은 우선순위(priority=0)
    breakAndDispatchFault(head, broker, 10);
    breakAndDispatchFault(pack, broker, 10);

    Technician t("T1", /*repairTime=*/2, broker);
    dispatcher.registerTechnician(&t);

    dispatcher.update(11);
    EXPECT_EQ(t.getTargetMachine(), &pack);
}

// ─────────────────────────────────────────────────────────────
// 3. FIFO 동률: 동일 priority 두 머신 → seq 작은 쪽 먼저
// ─────────────────────────────────────────────────────────────

TEST(PhaseRepairDispatcher, FifoTieBreakBySeq) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine head("M_HEAD", MachineType::HeadCutter, broker, rng, sharedIdGen());
    SpyMachine neck("M_NECK", MachineType::NeckCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&head);
    lookup.add(&neck);
    RepairDispatcher dispatcher(broker, lookup);
    dispatcher.setPriorityMap({{"M_HEAD", 3}, {"M_NECK", 3}});

    breakAndDispatchFault(head, broker, 10);
    breakAndDispatchFault(neck, broker, 10);

    Technician t("T1", 2, broker);
    dispatcher.registerTechnician(&t);

    dispatcher.update(11);
    EXPECT_EQ(t.getTargetMachine(), &head);
}

// ─────────────────────────────────────────────────────────────
// 4. idle Technician에 assign — 큐에서 pop
// ─────────────────────────────────────────────────────────────

TEST(PhaseRepairDispatcher, AssignsToIdleTechnicianAndPopsQueue) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m1("M1", MachineType::HeadCutter, broker, rng, sharedIdGen());
    SpyMachine m2("M2", MachineType::NeckCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&m1);
    lookup.add(&m2);
    RepairDispatcher dispatcher(broker, lookup);
    dispatcher.setPriorityMap({{"M1", 3}, {"M2", 3}});

    breakAndDispatchFault(m1, broker, 10);
    breakAndDispatchFault(m2, broker, 10);

    Technician t1("T1", 2, broker);
    Technician t2("T2", 2, broker);
    dispatcher.registerTechnician(&t1);
    dispatcher.registerTechnician(&t2);

    dispatcher.update(11);

    EXPECT_TRUE(dispatcher.getQueue().empty());
    EXPECT_FALSE(t1.isIdle());
    EXPECT_FALSE(t2.isIdle());
}

TEST(PhaseRepairDispatcher, BusyTechnicianNotReassigned) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m1("M1", MachineType::HeadCutter, broker, rng, sharedIdGen());
    SpyMachine m2("M2", MachineType::NeckCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&m1);
    lookup.add(&m2);
    RepairDispatcher dispatcher(broker, lookup);
    dispatcher.setPriorityMap({{"M1", 3}, {"M2", 3}});

    Technician t1("T1", /*repairTime=*/5, broker);
    dispatcher.registerTechnician(&t1);

    breakAndDispatchFault(m1, broker, 1);
    dispatcher.update(2);
    ASSERT_FALSE(t1.isIdle());

    breakAndDispatchFault(m2, broker, 3);
    dispatcher.update(4);

    ASSERT_EQ(dispatcher.getQueue().size(), 1u);
    EXPECT_EQ(dispatcher.getQueue()[0].machine, &m2);
    EXPECT_EQ(t1.getTargetMachine(), &m1);
}

// ─────────────────────────────────────────────────────────────
// 5. instantRepair 우회: 큐 상단 머신이 이미 idle이면 pop
// ─────────────────────────────────────────────────────────────

TEST(PhaseRepairDispatcher, StaleQueueHeadIsPoppedAutomatically) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m1("M1", MachineType::HeadCutter, broker, rng, sharedIdGen());
    SpyMachine m2("M2", MachineType::NeckCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&m1);
    lookup.add(&m2);
    RepairDispatcher dispatcher(broker, lookup);
    dispatcher.setPriorityMap({{"M1", 3}, {"M2", 3}});

    breakAndDispatchFault(m1, broker, 1);
    breakAndDispatchFault(m2, broker, 2);
    ASSERT_EQ(dispatcher.getQueue().size(), 2u);

    m1.repair(3);
    broker.flush();
    ASSERT_NE(m1.getCurrentState(), &MachineBrokenState::instance());

    Technician t1("T1", 2, broker);
    dispatcher.registerTechnician(&t1);

    dispatcher.update(4);
    EXPECT_EQ(t1.getTargetMachine(), &m2);
    EXPECT_TRUE(dispatcher.getQueue().empty());
}

// ─────────────────────────────────────────────────────────────
// 6. clearQueue / restoreQueue (메멘토 복원 시뮬레이션)
// ─────────────────────────────────────────────────────────────

TEST(PhaseRepairDispatcher, ClearAndRestoreQueue) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m("M1", MachineType::HeadCutter, broker, rng, sharedIdGen());

    StubLookup lookup;
    lookup.add(&m);
    RepairDispatcher dispatcher(broker, lookup);
    dispatcher.setPriorityMap({{"M1", 3}});

    breakAndDispatchFault(m, broker, 1);
    ASSERT_EQ(dispatcher.getQueue().size(), 1u);

    dispatcher.clearQueue();
    EXPECT_TRUE(dispatcher.getQueue().empty());
    EXPECT_EQ(dispatcher.getNextSeq(), 0);

    std::vector<RepairDispatcher::QueueEntry> restored = {
        {&m, /*priority=*/3, /*faultTick=*/5, /*seq=*/42},
    };
    dispatcher.restoreQueue(restored, /*nextSeq=*/43);
    ASSERT_EQ(dispatcher.getQueue().size(), 1u);
    EXPECT_EQ(dispatcher.getQueue()[0].seq, 42);
    EXPECT_EQ(dispatcher.getNextSeq(), 43);
}
