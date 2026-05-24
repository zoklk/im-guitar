// Phase 4 — Technician + State 패턴 검증.
// SpyMachine 스텁(Machine 자식)으로 Technician의 assign / Working 진입 /
// repairProgress 카운트 / repairTime 도달 시 machine.repair() 호출 / Idle 복귀를
// 관찰 가능한 health 상태 변화로 검증한다.

#include <memory>
#include <random>
#include <string>
#include <utility>

#include "common/Event.h"
#include "common/Types.h"
#include "model/event/EventBroker.h"
#include "model/machine/Machine.h"
#include "model/product/ProductIdGen.h"
#include "model/technician/Technician.h"
#include "model/technician/TechnicianStates.h"

#include <gtest/gtest.h>

namespace {

// SpyMachine: Machine 자식 스텁. process는 no-op. repair 호출은 getHealth()로 관찰.
// (Machine::repair는 non-virtual이라 직접 hook 못 함 → health 변화로 간접 관찰)
class SpyMachine : public Machine {
public:
    SpyMachine(std::string id, EventBroker& broker, std::mt19937& rng, ProductIdGen& idGen)
        : Machine(std::move(id), MachineType::HeadCutter,
                  /*processingTime=*/1, /*bp=*/0.0, /*requiredCount=*/1,
                  OverflowMode::Drop, broker, rng, idGen) {}

    void process(int /*tick*/) override { /* unused — Technician 테스트는 process 안 탐 */ }
};

// EventRecorder: Resume publish 검증용 (선택)
struct EventRecorder : public IEventHandler {
    int resumeCount = 0;
    std::string lastSourceId;
    int         lastTick = -1;
    void handle(const Event& ev) override {
        if (ev.type == EventType::Resume) {
            ++resumeCount;
            lastSourceId = ev.sourceId;
            lastTick     = ev.tick;
        }
    }
};

std::mt19937 makeRng(uint32_t seed = 42) { return std::mt19937(seed); }
ProductIdGen& sharedIdGen() {
    static ProductIdGen g;
    return g;
}

}  // namespace

// ─────────────────────────────────────────────────────────────
// 1. Idle 시작 상태
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechnician, StartsInIdleStateWithNoTarget) {
    EventBroker broker;
    Technician t("T1", /*repairTime=*/3, broker);

    EXPECT_TRUE(t.isIdle());
    EXPECT_EQ(t.getCurrentState(), &TechnicianIdleState::instance());
    EXPECT_EQ(t.getTargetMachine(), nullptr);
    EXPECT_EQ(t.getRepairProgress(), 0);
    EXPECT_EQ(t.getRepairTime(), 3);
}

TEST(PhaseTechnician, IdleUpdateIsNoOp) {
    EventBroker broker;
    Technician t("T1", 3, broker);

    t.update(1);
    t.update(2);
    t.update(3);

    EXPECT_TRUE(t.isIdle());
    EXPECT_EQ(t.getTargetMachine(), nullptr);
    EXPECT_EQ(t.getRepairProgress(), 0);
}

// ─────────────────────────────────────────────────────────────
// 2. assign → Working 진입 + onEnter에서 repairProgress 리셋
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechnician, AssignTransitionsToWorkingAndResetsProgress) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m("M1", broker, rng, sharedIdGen());
    Technician t("T1", 3, broker);

    t.assign(&m, /*tick=*/10);

    EXPECT_FALSE(t.isIdle());
    EXPECT_EQ(t.getCurrentState(), &TechnicianWorkingState::instance());
    EXPECT_EQ(t.getTargetMachine(), &m);
    EXPECT_EQ(t.getRepairProgress(), 0);   // onEnter가 0으로 리셋
}

// ─────────────────────────────────────────────────────────────
// 3. 1~2틱 진행: repairProgress 증가, repair 미호출
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechnician, ProgressIncrementsWithoutTriggeringRepairBeforeRepairTime) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m("M1", broker, rng, sharedIdGen());
    Technician t("T1", /*repairTime=*/3, broker);

    m.forceBreak();   // health = 0 (관찰용 초기 상태)
    ASSERT_EQ(m.getHealth(), 0);

    t.assign(&m, /*tick=*/10);

    t.update(11);
    EXPECT_EQ(t.getRepairProgress(), 1);
    EXPECT_EQ(m.getHealth(), 0);          // 아직 repair 미호출
    EXPECT_FALSE(t.isIdle());

    t.update(12);
    EXPECT_EQ(t.getRepairProgress(), 2);
    EXPECT_EQ(m.getHealth(), 0);
    EXPECT_FALSE(t.isIdle());
}

// ─────────────────────────────────────────────────────────────
// 4. repairTime 도달: machine.repair 호출 + targetMachine null + Idle 복귀
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechnician, ReachesRepairTimeTriggersRepairAndReturnsToIdle) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m("M1", broker, rng, sharedIdGen());
    Technician t("T1", /*repairTime=*/3, broker);

    m.forceBreak();
    t.assign(&m, /*tick=*/10);

    t.update(11);   // progress 1
    t.update(12);   // progress 2
    t.update(13);   // progress 3 → repair 호출 → Idle 복귀

    // repair 호출되었는지 = health 복원되었는지로 관찰
    EXPECT_EQ(m.getHealth(), m.getMaxHealth());
    EXPECT_TRUE(t.isIdle());
    EXPECT_EQ(t.getTargetMachine(), nullptr);
    EXPECT_EQ(t.getCurrentState(), &TechnicianIdleState::instance());
}

TEST(PhaseTechnician, RepairCompletionPublishesResumeAtCompletionTick) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m("M1", broker, rng, sharedIdGen());
    Technician t("T1", /*repairTime=*/3, broker);

    EventRecorder rec;
    broker.subscribe(EventType::Resume, &rec);

    m.forceBreak();
    t.assign(&m, /*tick=*/10);

    t.update(11);
    t.update(12);
    t.update(13);   // 완료 시점 tick=13
    broker.flush();

    EXPECT_EQ(rec.resumeCount, 1);
    EXPECT_EQ(rec.lastSourceId, "M1");    // Machine.repair가 sourceId=machine.id로 발행
    EXPECT_EQ(rec.lastTick, 13);          // 완료 시점 tick
}

// ─────────────────────────────────────────────────────────────
// 5. 재배정 가능: 한 사이클 완료 후 다음 머신 assign
// ─────────────────────────────────────────────────────────────

TEST(PhaseTechnician, CanReassignToAnotherMachineAfterCompletion) {
    EventBroker broker;
    auto rng = makeRng();
    SpyMachine m1("M1", broker, rng, sharedIdGen());
    SpyMachine m2("M2", broker, rng, sharedIdGen());
    Technician t("T1", /*repairTime=*/2, broker);

    m1.forceBreak();
    m2.forceBreak();

    // 1차 사이클
    t.assign(&m1, 1);
    t.update(2);   // progress 1
    t.update(3);   // progress 2 → 완료
    ASSERT_TRUE(t.isIdle());
    ASSERT_EQ(m1.getHealth(), m1.getMaxHealth());

    // 2차 사이클
    t.assign(&m2, 4);
    EXPECT_FALSE(t.isIdle());
    EXPECT_EQ(t.getTargetMachine(), &m2);
    EXPECT_EQ(t.getRepairProgress(), 0);   // onEnter 재실행

    t.update(5);
    t.update(6);
    EXPECT_TRUE(t.isIdle());
    EXPECT_EQ(m2.getHealth(), m2.getMaxHealth());
}
