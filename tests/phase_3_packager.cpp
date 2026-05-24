// Phase 3 Slice 5 — Packager (sink).
// FinishedGuitar 소비, 출력 없음. Packaged + Completed publish.
// Statistics 통합 시 wip 5→0 검증 (FinishedGuitar sourceCount=5).

#include <memory>
#include <random>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/event/EventBroker.h"
#include "model/machine/MachineStates.h"
#include "model/machine/packager/Packager.h"
#include "model/product/Product.h"
#include "model/product/ProductIdGen.h"
#include "model/stats/Statistics.h"

#include <gtest/gtest.h>

namespace {

struct EventRecorder : public IEventHandler {
    std::vector<Event> received;
    void handle(const Event& ev) override { received.push_back(ev); }
};

std::mt19937 makeRng(uint32_t seed = 42) { return std::mt19937(seed); }

}  // namespace

// ─────────────────────────────────────────────────────────────
// 1. 기본 동작 — FinishedGuitar 소비, outputCount 증가
// ─────────────────────────────────────────────────────────────

TEST(PhasePackager, ConsumesFinishedGuitarAndIncrementsOutputCount) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Packager     p("PKG1", /*pt=*/1, 0.0, broker, rng, idGen);

    p.acceptProduct(std::make_unique<FinishedGuitar>(42));

    for (int t = 1; t <= 2; ++t) p.update(t);

    EXPECT_EQ(p.getCurrentState(), &MachineIdleState::instance());
    EXPECT_EQ(p.getOutputCount(), 1);
    EXPECT_EQ(p.getInputBufferSize(), 0);
}

// ─────────────────────────────────────────────────────────────
// 2. 이벤트 발행 — Started / Completed / Packaged 모두
// ─────────────────────────────────────────────────────────────

TEST(PhasePackager, PublishesStartedCompletedAndPackagedEvents) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;
    Packager     p("PKG1", 1, 0.0, broker, rng, idGen);

    p.acceptProduct(std::make_unique<FinishedGuitar>(42));
    for (int t = 1; t <= 2; ++t) p.update(t);
    broker.flush();

    int started = 0, completed = 0, packaged = 0;
    for (const auto& ev : rec.received) {
        if (ev.sourceId != "PKG1") continue;
        if (ev.type == EventType::Started) {
            ++started;
            EXPECT_EQ(ev.productType, ProductType::FinishedGuitar);
            EXPECT_EQ(*ev.productId, 42);
        }
        if (ev.type == EventType::Completed) {
            ++completed;
            EXPECT_EQ(ev.productType, ProductType::FinishedGuitar);
            EXPECT_EQ(*ev.productId, 42);
        }
        if (ev.type == EventType::Packaged) {
            ++packaged;
            EXPECT_EQ(ev.productType, ProductType::FinishedGuitar);
            EXPECT_EQ(*ev.productId, 42);
        }
    }
    EXPECT_EQ(started, 1);
    EXPECT_EQ(completed, 1);
    EXPECT_EQ(packaged, 1);
}

// ─────────────────────────────────────────────────────────────
// 3. outputConveyor 없이도 정상 동작 (sink 패턴)
// ─────────────────────────────────────────────────────────────

TEST(PhasePackager, OperatesWithoutOutputConveyor) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Packager     p("PKG1", 1, 0.0, broker, rng, idGen);

    EXPECT_EQ(p.getOutputConveyor(), nullptr);

    p.acceptProduct(std::make_unique<FinishedGuitar>(1));
    // outputConveyor null이라도 abort 없이 정상 처리
    for (int t = 1; t <= 2; ++t) p.update(t);

    EXPECT_EQ(p.getOutputCount(), 1);
}

// ─────────────────────────────────────────────────────────────
// 4. canStart — input 있으면 true
// ─────────────────────────────────────────────────────────────

TEST(PhasePackager, CanStartTrueOnlyWithInputPresent) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Packager     p("PKG1", 1, 0.0, broker, rng, idGen);

    EXPECT_FALSE(p.canStart());
    p.acceptProduct(std::make_unique<FinishedGuitar>(1));
    EXPECT_TRUE(p.canStart());
}

// ─────────────────────────────────────────────────────────────
// 5. 다중 사이클 — queued guitars 순차 처리
// ─────────────────────────────────────────────────────────────

TEST(PhasePackager, ProcessesQueuedGuitarsAcrossCycles) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Packager     p("PKG1", 1, 0.0, broker, rng, idGen);

    p.acceptProduct(std::make_unique<FinishedGuitar>(1));
    p.acceptProduct(std::make_unique<FinishedGuitar>(2));
    p.acceptProduct(std::make_unique<FinishedGuitar>(3));

    // 3 cycles = 6 updates (1 + 1 per cycle)
    for (int t = 1; t <= 6; ++t) p.update(t);

    EXPECT_EQ(p.getOutputCount(), 3);
    EXPECT_EQ(p.getInputBufferSize(), 0);
}

// ─────────────────────────────────────────────────────────────
// 6. 통합 — Statistics가 Packaged로 wip -= 5, finished++
// ─────────────────────────────────────────────────────────────

TEST(PhasePackager, IntegrationStatisticsWipDecrementsBySourceCount5) {
    EventBroker  broker;
    Statistics   stats(broker);
    auto         rng = makeRng();
    ProductIdGen idGen;
    Packager     p("PKG1", 1, 0.0, broker, rng, idGen);

    // 사전 5번 Spawned (5 raw 자재) → wip=5
    for (int i = 1; i <= 5; ++i) {
        Event ev;
        ev.type        = EventType::Spawned;
        ev.sourceId    = "Spawner";
        ev.tick        = 0;
        ev.productId   = i;
        ev.productType = ProductType::RawWood;
        broker.publish(ev);
    }
    broker.flush();
    ASSERT_EQ(stats.getWip(), 5);

    p.acceptProduct(std::make_unique<FinishedGuitar>(100));
    for (int t = 1; t <= 2; ++t) p.update(t);
    broker.flush();

    EXPECT_EQ(stats.getFinished(), 1);
    EXPECT_EQ(stats.getWip(), 0);          // 5 spawn → 1 guitar 출하로 net 0
    EXPECT_EQ(stats.getLost(), 0);
    EXPECT_EQ(stats.getBreakdowns(), 0);
}
