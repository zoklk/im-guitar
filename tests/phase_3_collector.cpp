// Phase 3 Slice 4 — ElecPartCollector (Bridge + Pickup → ElecPartSet).
// MultiInputMachine 동작: typedBuffer 분류, canStart 종류별 검사, gatherInputs.

#include <memory>
#include <random>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/conveyor/Conveyor.h"
#include "model/event/EventBroker.h"
#include "model/machine/MachineStates.h"
#include "model/machine/multiple/collector/ElecPartCollector.h"
#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"

#include <gtest/gtest.h>

namespace {

struct EventRecorder : public IEventHandler {
    std::vector<Event> received;
    void handle(const Event& ev) override { received.push_back(ev); }
};

std::mt19937 makeRng(uint32_t seed = 42) { return std::mt19937(seed); }

}  // namespace

// ─────────────────────────────────────────────────────────────
// 1. canStart는 두 종류 모두 있어야 true
// ─────────────────────────────────────────────────────────────

TEST(PhaseCollector, CanStartFalseWithOnlyBridge) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    ElecPartCollector c("EPC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    c.setOutputConveyor(&out);

    c.acceptProduct(std::make_unique<Bridge>(1));
    EXPECT_FALSE(c.canStart());

    // 1틱 진행해도 Idle 유지
    c.update(1);
    EXPECT_EQ(c.getCurrentState(), &MachineIdleState::instance());
}

TEST(PhaseCollector, CanStartFalseWithOnlyPickup) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    ElecPartCollector c("EPC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    c.setOutputConveyor(&out);

    c.acceptProduct(std::make_unique<Pickup>(1));
    EXPECT_FALSE(c.canStart());
}

TEST(PhaseCollector, CanStartTrueWithBothTypes) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    ElecPartCollector c("EPC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    c.setOutputConveyor(&out);

    c.acceptProduct(std::make_unique<Bridge>(1));
    c.acceptProduct(std::make_unique<Pickup>(2));
    EXPECT_TRUE(c.canStart());
}

// ─────────────────────────────────────────────────────────────
// 2. typedBuffer 분류 — getInputBufferSize는 두 큐 합
// ─────────────────────────────────────────────────────────────

TEST(PhaseCollector, InputBufferSizeIsSumAcrossTypes) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    ElecPartCollector c("EPC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    c.setOutputConveyor(&out);

    c.acceptProduct(std::make_unique<Bridge>(1));
    c.acceptProduct(std::make_unique<Bridge>(2));
    c.acceptProduct(std::make_unique<Pickup>(3));
    EXPECT_EQ(c.getInputBufferSize(), 3);
}

// ─────────────────────────────────────────────────────────────
// 3. process → ElecPartSet 생성 (새 id), 두 input 모두 소비
// ─────────────────────────────────────────────────────────────

TEST(PhaseCollector, ProducesElecPartSetConsumingOneOfEachType) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    ElecPartCollector c("EPC1", /*pt=*/1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    c.setOutputConveyor(&out);

    c.acceptProduct(std::make_unique<Bridge>(10));
    c.acceptProduct(std::make_unique<Pickup>(20));
    c.acceptProduct(std::make_unique<Bridge>(11));   // 잔여 (다음 사이클용)

    for (int t = 1; t <= 2; ++t) c.update(t);   // 1 cycle

    EXPECT_EQ(c.getOutputCount(), 1);
    EXPECT_EQ(c.getInputBufferSize(), 1);       // Bridge 1개 잔여
    ASSERT_NE(out.slotAt(0), nullptr);
    EXPECT_EQ(out.slotAt(0)->getType(), ProductType::ElecPartSet);
    EXPECT_EQ(out.slotAt(0)->getId(), 1);       // idGen 첫 발급
}

// ─────────────────────────────────────────────────────────────
// 4. 이벤트 발행 — Started + Completed
// ─────────────────────────────────────────────────────────────

TEST(PhaseCollector, PublishesStartedAndCompletedAroundProcess) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    ElecPartCollector c("EPC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    c.setOutputConveyor(&out);

    c.acceptProduct(std::make_unique<Bridge>(10));
    c.acceptProduct(std::make_unique<Pickup>(20));
    for (int t = 1; t <= 2; ++t) c.update(t);
    broker.flush();

    int started = 0, completed = 0;
    for (const auto& ev : rec.received) {
        if (ev.sourceId != "EPC1") continue;
        if (ev.type == EventType::Started)   ++started;
        if (ev.type == EventType::Completed) {
            ++completed;
            EXPECT_EQ(ev.productType, ProductType::ElecPartSet);
        }
    }
    EXPECT_EQ(started, 1);
    EXPECT_EQ(completed, 1);
}

// ─────────────────────────────────────────────────────────────
// 5. Drop 모드 — output (ElecPartSet) 손실
// ─────────────────────────────────────────────────────────────

TEST(PhaseCollector, DropModePublishesDropOnFullConveyor) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 1, broker);
    ElecPartCollector c("EPC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    c.setOutputConveyor(&out);

    out.push(std::make_unique<RawWood>(999), 0);

    c.acceptProduct(std::make_unique<Bridge>(10));
    c.acceptProduct(std::make_unique<Pickup>(20));
    for (int t = 1; t <= 2; ++t) c.update(t);

    EXPECT_EQ(c.getOutputCount(), 0);

    broker.flush();
    int drop = 0;
    for (const auto& ev : rec.received) {
        if (ev.sourceId == "EPC1" && ev.type == EventType::Drop) {
            ++drop;
            EXPECT_EQ(ev.productType, ProductType::ElecPartSet);
        }
    }
    EXPECT_EQ(drop, 1);
}

// ─────────────────────────────────────────────────────────────
// 6. Backpressure 모드 — input 보존, process 차단
// ─────────────────────────────────────────────────────────────

TEST(PhaseCollector, BackpressureModePreservesInputsAndBlocks) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 1, broker);
    ElecPartCollector c("EPC1", 1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    c.setOutputConveyor(&out);

    out.push(std::make_unique<RawWood>(999), 0);

    c.acceptProduct(std::make_unique<Bridge>(10));
    c.acceptProduct(std::make_unique<Pickup>(20));
    for (int t = 1; t <= 5; ++t) c.update(t);

    EXPECT_EQ(c.getCurrentState(), &MachineIdleState::instance());
    EXPECT_EQ(c.getInputBufferSize(), 2);   // 두 input 모두 보존
    EXPECT_EQ(c.getOutputCount(), 0);
}
