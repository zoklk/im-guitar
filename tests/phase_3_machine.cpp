// Phase 3 Slice 1 — State 패턴 + Machine 추상 골격 검증.
// TestMachine 스텁으로 상태 전이, health drop, forceBreak/repair, tryPushOrDrop Drop,
// Fault cascade handle() 동작을 검증한다. 구체 머신 (Spawner/Cutter/...)은 후속 슬라이스.

#include <memory>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/conveyor/Conveyor.h"
#include "model/event/EventBroker.h"
#include "model/machine/Machine.h"
#include "model/machine/MachineStates.h"
#include "model/product/Product.h"

#include <gtest/gtest.h>

namespace {

// 단순 1→1 변환 머신 스텁. process()는 currentProduct에서 1개 꺼내
// HeadPart(id 고정)으로 변환 후 tryPushOrDrop. Started/Completed publish 검증용.
class TestMachine : public Machine {
public:
    TestMachine(std::string   id,
                int           processingTime,
                double        breakdownProb,
                int           requiredCount,
                OverflowMode  mode,
                EventBroker&  broker,
                std::mt19937& rng,
                ProductIdGen& idGen)
        : Machine(std::move(id), MachineType::HeadCutter,
                  processingTime, breakdownProb, requiredCount, mode, broker, rng, idGen) {}

    int processCalls = 0;

    void process(int tick) override {
        ++processCalls;
        if (currentProduct_.empty()) return;
        // 변환 → HeadPart (id는 input의 id + 1000으로 단순 부여)
        auto in     = std::move(currentProduct_.back());
        currentProduct_.pop_back();
        auto out    = std::make_unique<HeadPart>(in->getId() + 1000);
        const int  pid = out->getId();
        const auto pt  = out->getType();
        if (tryPushOrDrop(std::move(out), tick)) {
            publishEvent(EventType::Completed, tick, pid, pt);
        }
        // Drop 경로면 tryPushOrDrop이 이미 Drop publish
    }
};

struct EventRecorder : public IEventHandler {
    std::vector<Event> received;
    void handle(const Event& ev) override { received.push_back(ev); }
};

std::mt19937 makeRng(uint32_t seed = 42) { return std::mt19937(seed); }

// Slice 1 TestMachine은 직접 idGen.next()를 호출하지 않으므로 공유 인스턴스로 충분.
// (TestMachine.process는 input id + 1000으로 단순 변환)
ProductIdGen& sharedIdGen() {
    static ProductIdGen g;
    return g;
}

}  // namespace

// ─────────────────────────────────────────────────────────────
// 1. 초기 상태 / Idle ↔ Processing 전이
// ─────────────────────────────────────────────────────────────

TEST(PhaseMachineSkeleton, StartsInIdleState) {
    EventBroker broker;
    auto        rng = makeRng();
    TestMachine m("M1", /*pt=*/3, /*bp=*/0.0, /*req=*/1, OverflowMode::Drop, broker, rng, sharedIdGen());

    EXPECT_EQ(m.getCurrentState(), &MachineIdleState::instance());
    EXPECT_EQ(m.getHealth(), 10);
    EXPECT_EQ(m.getProcessingTick(), 0);
}

TEST(PhaseMachineSkeleton, IdleStaysIdleWhenInputInsufficient) {
    EventBroker broker;
    auto        rng = makeRng();
    TestMachine m("M1", 3, 0.0, /*req=*/1, OverflowMode::Drop, broker, rng, sharedIdGen());

    m.update(1);
    EXPECT_EQ(m.getCurrentState(), &MachineIdleState::instance());
}

TEST(PhaseMachineSkeleton, IdleTransitionsToProcessingWhenCanStart) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto rng = makeRng();
    TestMachine m("M1", 3, 0.0, 1, OverflowMode::Drop, broker, rng, sharedIdGen());

    m.acceptProduct(std::make_unique<RawWood>(7));
    m.update(1);
    broker.flush();

    EXPECT_EQ(m.getCurrentState(), &MachineProcessingState::instance());
    EXPECT_EQ(m.getCurrentProductSize(), 1);
    EXPECT_EQ(m.getInputBufferSize(), 0);

    // ProcessingState.onEnter가 Started publish (productId=7)
    ASSERT_FALSE(rec.received.empty());
    EXPECT_EQ(rec.received[0].type, EventType::Started);
    EXPECT_EQ(rec.received[0].sourceId, "M1");
    ASSERT_TRUE(rec.received[0].productId.has_value());
    EXPECT_EQ(*rec.received[0].productId, 7);
}

// ─────────────────────────────────────────────────────────────
// 2. Processing → process() → Idle 복귀
// ─────────────────────────────────────────────────────────────

TEST(PhaseMachineSkeleton, ProcessingCompletesAfterProcessingTime) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto rng = makeRng();
    TestMachine m("M1", /*pt=*/3, 0.0, 1, OverflowMode::Drop, broker, rng, sharedIdGen());

    Conveyor out("Cout", 3, broker);
    m.setOutputConveyor(&out);

    m.acceptProduct(std::make_unique<RawWood>(7));
    m.update(1);     // Idle → Processing.onEnter (Started publish), tick=1 internal
    m.update(2);     // tick=1 (processingTick++)
    m.update(3);     // tick=2
    m.update(4);     // tick=3 도달 → process 호출, Idle 복귀

    EXPECT_EQ(m.getCurrentState(), &MachineIdleState::instance());
    EXPECT_EQ(m.processCalls, 1);
    EXPECT_EQ(m.getOutputCount(), 1);

    broker.flush();
    // Started + Completed (push 성공)
    int started = 0, completed = 0;
    for (const auto& ev : rec.received) {
        if (ev.type == EventType::Started)   ++started;
        if (ev.type == EventType::Completed) ++completed;
    }
    EXPECT_EQ(started, 1);
    EXPECT_EQ(completed, 1);
}

// ─────────────────────────────────────────────────────────────
// 3. tryPushOrDrop — 가득찬 conveyor (Drop 모드)
// ─────────────────────────────────────────────────────────────

TEST(PhaseMachineSkeleton, TryPushOrDropPublishesDropWhenFull) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto rng = makeRng();
    TestMachine m("M1", 1, 0.0, 1, OverflowMode::Drop, broker, rng, sharedIdGen());

    Conveyor out("Cout", 1, broker);
    m.setOutputConveyor(&out);

    // 미리 conveyor 출구 슬롯을 막아두기 위해 1개 push (downstream null이라 머무름)
    out.push(std::make_unique<RawWood>(99), 0);
    ASSERT_FALSE(out.canAccept());

    m.acceptProduct(std::make_unique<RawWood>(7));
    m.update(1);  // Drop 모드라 canStart=true (canAccept 무시) → Processing.onEnter
    m.update(2);  // pt=1 도달 → process → tryPushOrDrop → Drop publish

    EXPECT_EQ(m.getCurrentState(), &MachineIdleState::instance());
    EXPECT_EQ(m.getOutputCount(), 0);   // push 실패

    broker.flush();
    int drops = 0, completed = 0;
    for (const auto& ev : rec.received) {
        if (ev.type == EventType::Drop)      { ++drops; EXPECT_EQ(ev.sourceId, "M1"); }
        if (ev.type == EventType::Completed) { ++completed; }
    }
    EXPECT_EQ(drops, 1);
    EXPECT_EQ(completed, 0);   // push 실패 시 Completed 미발행
}

// ─────────────────────────────────────────────────────────────
// 4. Backpressure 모드 — canStart가 사전 차단
// ─────────────────────────────────────────────────────────────

TEST(PhaseMachineSkeleton, BackpressureModeBlocksStartWhenConveyorFull) {
    EventBroker broker;
    auto        rng = makeRng();
    TestMachine m("M1", 1, 0.0, 1, OverflowMode::Backpressure, broker, rng, sharedIdGen());

    Conveyor out("Cout", 1, broker);
    m.setOutputConveyor(&out);

    out.push(std::make_unique<RawWood>(99), 0);
    ASSERT_FALSE(out.canAccept());

    m.acceptProduct(std::make_unique<RawWood>(7));
    m.update(1);  // Backpressure + !canAccept → canStart false → Idle 머무름

    EXPECT_EQ(m.getCurrentState(), &MachineIdleState::instance());
    EXPECT_TRUE(m.isSuspendedByBackpressure());
}

// ─────────────────────────────────────────────────────────────
// 5. Health drop — breakdownProb=1.0 → 즉시 -1
// ─────────────────────────────────────────────────────────────

TEST(PhaseMachineSkeleton, BreakdownProb1DecrementsHealthEveryTick) {
    EventBroker broker;
    auto        rng = makeRng();
    TestMachine m("M1", /*pt=*/100, /*bp=*/1.0, 1, OverflowMode::Drop, broker, rng, sharedIdGen());

    m.acceptProduct(std::make_unique<RawWood>(1));
    m.update(1);  // Idle → Processing (onEnter, no health check)
    // 그다음 update마다 health 1씩 감소 (bp=1.0 → 항상 적중)
    for (int t = 2; t <= 11; ++t) {
        m.update(t);
    }
    // 10번 update → health 0 도달 → Broken
    EXPECT_EQ(m.getHealth(), 0);
    EXPECT_EQ(m.getCurrentState(), &MachineBrokenState::instance());
}

// ─────────────────────────────────────────────────────────────
// 6. forceBreak → 다음 update에서 Broken + Fault
// ─────────────────────────────────────────────────────────────

TEST(PhaseMachineSkeleton, ForceBreakLeadsToBrokenStateOnNextUpdate) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto rng = makeRng();
    TestMachine m("M1", 5, 0.0, 1, OverflowMode::Drop, broker, rng, sharedIdGen());

    m.forceBreak();
    EXPECT_EQ(m.getHealth(), 0);
    EXPECT_EQ(m.getCurrentState(), &MachineIdleState::instance());   // 아직 전이 안 됨

    m.update(7);
    broker.flush();

    EXPECT_EQ(m.getCurrentState(), &MachineBrokenState::instance());
    bool faultPublished = false;
    for (const auto& ev : rec.received) {
        if (ev.type == EventType::Fault && ev.sourceId == "M1" && ev.tick == 7) {
            faultPublished = true;
        }
    }
    EXPECT_TRUE(faultPublished);
}

// ─────────────────────────────────────────────────────────────
// 7. repair → health 복원 + Resume publish + 상태 복귀
// ─────────────────────────────────────────────────────────────

TEST(PhaseMachineSkeleton, RepairFromBrokenWithProcessingTickReturnsToProcessing) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto rng = makeRng();
    // bp=1.0: health 매 틱 -1. processingTime 충분히 크게 잡아 health 먼저 0 도달
    TestMachine m("M1", /*pt=*/100, /*bp=*/1.0, 1, OverflowMode::Drop, broker, rng, sharedIdGen());

    Conveyor out("Cout", 3, broker);
    m.setOutputConveyor(&out);

    m.acceptProduct(std::make_unique<RawWood>(1));
    m.update(1);   // Processing.onEnter
    // health 10에서 매 update -1, 10번 update로 0 도달 → Broken 전이
    for (int t = 2; t <= 11; ++t) {
        m.update(t);
    }
    ASSERT_EQ(m.getCurrentState(), &MachineBrokenState::instance());
    const int savedTick = m.getProcessingTick();
    EXPECT_GT(savedTick, 0);                          // 처리 중 깨졌으므로 processingTick 보존
    EXPECT_EQ(m.getCurrentProductSize(), 1);          // currentProduct도 보존

    m.repair(20);
    broker.flush();

    EXPECT_EQ(m.getHealth(), 10);
    EXPECT_EQ(m.getCurrentState(), &MachineProcessingState::instance());
    EXPECT_EQ(m.getProcessingTick(), savedTick);      // resume 후에도 processingTick 그대로

    bool resumePublished = false;
    for (const auto& ev : rec.received) {
        if (ev.type == EventType::Resume && ev.sourceId == "M1" && ev.tick == 20) {
            resumePublished = true;
        }
    }
    EXPECT_TRUE(resumePublished);
}

TEST(PhaseMachineSkeleton, RepairFromBrokenIdleReturnsToIdle) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto rng = makeRng();
    TestMachine m("M1", 5, 0.0, 1, OverflowMode::Drop, broker, rng, sharedIdGen());

    // Idle에서 forceBreak → 다음 update에서 Broken
    m.forceBreak();
    m.update(1);
    ASSERT_EQ(m.getCurrentState(), &MachineBrokenState::instance());
    EXPECT_EQ(m.getProcessingTick(), 0);

    m.repair(5);
    broker.flush();

    EXPECT_EQ(m.getHealth(), 10);
    EXPECT_EQ(m.getCurrentState(), &MachineIdleState::instance());

    bool resumePublished = false;
    for (const auto& ev : rec.received) {
        if (ev.type == EventType::Resume && ev.sourceId == "M1" && ev.tick == 5) {
            resumePublished = true;
        }
    }
    EXPECT_TRUE(resumePublished);
}

// ─────────────────────────────────────────────────────────────
// 8. Fault cascade handle() — Fault++ / Resume--
// ─────────────────────────────────────────────────────────────

TEST(PhaseMachineSkeleton, HandleFaultIncrementsPendingDownstreamFaults) {
    EventBroker broker;
    auto        rng = makeRng();
    TestMachine m("M1", 5, 0.0, 1, OverflowMode::Drop, broker, rng, sharedIdGen());

    Event ev{};
    ev.type     = EventType::Fault;
    ev.sourceId = "downstream";
    m.handle(ev);
    m.handle(ev);
    EXPECT_EQ(m.getPendingDownstreamFaults(), 2);

    ev.type = EventType::Resume;
    m.handle(ev);
    EXPECT_EQ(m.getPendingDownstreamFaults(), 1);
    m.handle(ev);
    EXPECT_EQ(m.getPendingDownstreamFaults(), 0);
    m.handle(ev);   // 0 이하로 안 내려감
    EXPECT_EQ(m.getPendingDownstreamFaults(), 0);
}

TEST(PhaseMachineSkeleton, CanStartFalseWhenPendingDownstreamFaultsPositive) {
    EventBroker broker;
    auto        rng = makeRng();
    TestMachine m("M1", 5, 0.0, 1, OverflowMode::Drop, broker, rng, sharedIdGen());

    m.acceptProduct(std::make_unique<RawWood>(1));
    EXPECT_TRUE(m.canStart());

    Event ev{};
    ev.type = EventType::Fault;
    m.handle(ev);
    EXPECT_FALSE(m.canStart());

    ev.type = EventType::Resume;
    m.handle(ev);
    EXPECT_TRUE(m.canStart());
}
