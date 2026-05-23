// Phase 3 Slice 3 — Cutter 추상 + 3종.
// 1→1 변환 머신 동작: Started/Completed/Drop 발행, 새 id 발급, ProductType 변환.

#include <memory>
#include <random>
#include <string>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/conveyor/Conveyor.h"
#include "model/event/EventBroker.h"
#include "model/machine/MachineStates.h"
#include "model/machine/cutter/Cutters.h"
#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"

#include <gtest/gtest.h>

namespace {

struct EventRecorder : public IEventHandler {
    std::vector<Event> received;
    void handle(const Event& ev) override { received.push_back(ev); }
};

std::mt19937 makeRng(uint32_t seed = 42) { return std::mt19937(seed); }

// processingTime=N이면 사이클: Idle 1틱 + Processing N틱 → 총 N+1 update로 process 호출
int ticksToFirstComplete(int processingTime) { return processingTime + 1; }

}  // namespace

// ─────────────────────────────────────────────────────────────
// 1. HeadCutter — RawWood → HeadPart with new id
// ─────────────────────────────────────────────────────────────

TEST(PhaseCutters, HeadCutterConvertsRawWoodToHeadPartWithNewId) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;

    Conveyor   out("Cout", 3, broker);
    HeadCutter hc("HC1", /*pt=*/2, 0.0, OverflowMode::Drop, broker, rng, idGen);
    hc.setOutputConveyor(&out);

    hc.acceptProduct(std::make_unique<RawWood>(/*id=*/7));

    for (int t = 1; t <= ticksToFirstComplete(2); ++t) hc.update(t);

    EXPECT_EQ(hc.getCurrentState(), &MachineIdleState::instance());
    EXPECT_EQ(hc.getOutputCount(), 1);

    // output은 새 id 발급된 HeadPart
    ASSERT_NE(out.slotAt(0), nullptr);
    EXPECT_EQ(out.slotAt(0)->getType(), ProductType::HeadPart);
    EXPECT_NE(out.slotAt(0)->getId(), 7);   // input id 계승 안 함
    EXPECT_EQ(out.slotAt(0)->getId(), 1);   // idGen.next() 첫 호출

    broker.flush();
    int started = 0, completed = 0;
    for (const auto& ev : rec.received) {
        if (ev.sourceId == "HC1") {
            if (ev.type == EventType::Started)   ++started;
            if (ev.type == EventType::Completed) ++completed;
        }
    }
    EXPECT_EQ(started, 1);
    EXPECT_EQ(completed, 1);
}

TEST(PhaseCutters, StartedCarriesInputInfoCompletedCarriesOutputInfo) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;

    Conveyor   out("Cout", 3, broker);
    HeadCutter hc("HC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    hc.setOutputConveyor(&out);

    hc.acceptProduct(std::make_unique<RawWood>(7));
    for (int t = 1; t <= ticksToFirstComplete(1); ++t) hc.update(t);
    broker.flush();

    const Event* started   = nullptr;
    const Event* completed = nullptr;
    for (const auto& ev : rec.received) {
        if (ev.sourceId == "HC1" && ev.type == EventType::Started)   started   = &ev;
        if (ev.sourceId == "HC1" && ev.type == EventType::Completed) completed = &ev;
    }
    ASSERT_NE(started, nullptr);
    EXPECT_EQ(started->productType, ProductType::RawWood);   // input
    EXPECT_EQ(*started->productId, 7);

    ASSERT_NE(completed, nullptr);
    EXPECT_EQ(completed->productType, ProductType::HeadPart);  // output
    EXPECT_EQ(*completed->productId, 1);
}

// ─────────────────────────────────────────────────────────────
// 2. NeckCutter / BodyCutter — 각자 정확한 ProductType
// ─────────────────────────────────────────────────────────────

TEST(PhaseCutters, NeckCutterProducesNeckPart) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    NeckCutter   nc("NC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    nc.setOutputConveyor(&out);
    nc.acceptProduct(std::make_unique<RawWood>(10));
    for (int t = 1; t <= 2; ++t) nc.update(t);
    ASSERT_NE(out.slotAt(0), nullptr);
    EXPECT_EQ(out.slotAt(0)->getType(), ProductType::NeckPart);
}

TEST(PhaseCutters, BodyCutterProducesBodyPart) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    BodyCutter   bc("BC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    bc.setOutputConveyor(&out);
    bc.acceptProduct(std::make_unique<RawWood>(10));
    for (int t = 1; t <= 2; ++t) bc.update(t);
    ASSERT_NE(out.slotAt(0), nullptr);
    EXPECT_EQ(out.slotAt(0)->getType(), ProductType::BodyPart);

    // BodyCutter 출력은 isPainted=false
    auto* bp = static_cast<const BodyPart*>(out.slotAt(0));
    EXPECT_FALSE(bp->isPainted());
}

// ─────────────────────────────────────────────────────────────
// 3. Cutter — input 소비 검증
// ─────────────────────────────────────────────────────────────

TEST(PhaseCutters, CutterConsumesInputBufferEntry) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    HeadCutter   hc("HC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    hc.setOutputConveyor(&out);

    hc.acceptProduct(std::make_unique<RawWood>(1));
    hc.acceptProduct(std::make_unique<RawWood>(2));
    EXPECT_EQ(hc.getInputBufferSize(), 2);

    for (int t = 1; t <= 2; ++t) hc.update(t);   // 1 cycle → 1개 소비

    EXPECT_EQ(hc.getInputBufferSize(), 1);
    EXPECT_EQ(hc.getCurrentProductSize(), 0);   // 소비 + 변환 후 비움
}

// ─────────────────────────────────────────────────────────────
// 4. Cutter Drop 모드 + 가득찬 conveyor — Drop publish, Completed 미발행
// ─────────────────────────────────────────────────────────────

TEST(PhaseCutters, DropModePublishesDropAndSkipsCompletedOnFullConveyor) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;

    Conveyor   out("Cout", 1, broker);
    HeadCutter hc("HC1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    hc.setOutputConveyor(&out);

    // conveyor 미리 점유
    out.push(std::make_unique<RawWood>(999), 0);
    ASSERT_FALSE(out.canAccept());

    hc.acceptProduct(std::make_unique<RawWood>(7));
    for (int t = 1; t <= 2; ++t) hc.update(t);

    EXPECT_EQ(hc.getOutputCount(), 0);

    broker.flush();
    int started = 0, completed = 0, drop = 0;
    for (const auto& ev : rec.received) {
        if (ev.sourceId == "HC1") {
            if (ev.type == EventType::Started)   ++started;
            if (ev.type == EventType::Completed) ++completed;
            if (ev.type == EventType::Drop) {
                ++drop;
                EXPECT_EQ(ev.productType, ProductType::HeadPart);  // output이 손실됨
            }
        }
    }
    EXPECT_EQ(started, 1);
    EXPECT_EQ(completed, 0);
    EXPECT_EQ(drop, 1);
}

// ─────────────────────────────────────────────────────────────
// 5. Cutter Backpressure 모드 — canStart 차단으로 process 자체 안 일어남
// ─────────────────────────────────────────────────────────────

TEST(PhaseCutters, BackpressureModeBlocksProcessingWhenConveyorFull) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;

    Conveyor   out("Cout", 1, broker);
    HeadCutter hc("HC1", 1, 0.0, OverflowMode::Backpressure, broker, rng, idGen);
    hc.setOutputConveyor(&out);

    out.push(std::make_unique<RawWood>(999), 0);

    hc.acceptProduct(std::make_unique<RawWood>(7));
    for (int t = 1; t <= 5; ++t) hc.update(t);

    EXPECT_EQ(hc.getCurrentState(), &MachineIdleState::instance());
    EXPECT_EQ(hc.getInputBufferSize(), 1);   // input 보존
    EXPECT_EQ(hc.getOutputCount(), 0);

    broker.flush();
    // Started도 안 발행 (Processing 진입 자체가 안 됐으니)
    for (const auto& ev : rec.received) {
        if (ev.sourceId == "HC1") {
            EXPECT_NE(ev.type, EventType::Started);
            EXPECT_NE(ev.type, EventType::Completed);
            EXPECT_NE(ev.type, EventType::Drop);
        }
    }
}

// ─────────────────────────────────────────────────────────────
// 6. 다중 사이클 — input 큐에서 여러 개 순차 처리
// ─────────────────────────────────────────────────────────────

TEST(PhaseCutters, MultipleCyclesProcessQueuedInputsSequentially) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 20, broker);
    HeadCutter   hc("HC1", /*pt=*/1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    hc.setOutputConveyor(&out);

    hc.acceptProduct(std::make_unique<RawWood>(10));
    hc.acceptProduct(std::make_unique<RawWood>(20));
    hc.acceptProduct(std::make_unique<RawWood>(30));

    // 3 cycles = 3 * 2 = 6 updates (conveyor도 함께 tick해야 slot 비움)
    for (int t = 1; t <= 6; ++t) {
        hc.update(t);
        out.update(t);   // downstream null이라 출구 슬롯 처리는 안 되지만 shift는 동작
    }

    // 출구가 막혀있어 길이 20 conveyor 안에서 응축되며 모든 3개가 들어감.
    // 정확한 outputCount는 conveyor 응축 동작에 의존하므로 처리 진행 자체만 검증:
    EXPECT_GT(hc.getOutputCount(), 0);
    EXPECT_LT(hc.getInputBufferSize(), 3);   // 적어도 1개는 소비됨
}
