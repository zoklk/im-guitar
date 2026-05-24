// Phase 3 Slice 4 — BodyAssembler (3종) + PartAssembler (2종).
// Multi-input 동작: requiredTypes별 canStart, gatherInputs, output 생성.

#include <memory>
#include <random>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/conveyor/Conveyor.h"
#include "model/event/EventBroker.h"
#include "model/machine/MachineStates.h"
#include "model/machine/multiple/assembler/Assemblers.h"
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
// BodyAssembler — Head + Neck + Body → AssembledBody
// ─────────────────────────────────────────────────────────────

TEST(PhaseAssemblers, BodyAssemblerNeedsAllThreeTypesToStart) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    BodyAssembler ba("BA1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    ba.setOutputConveyor(&out);

    ba.acceptProduct(std::make_unique<HeadPart>(1));
    EXPECT_FALSE(ba.canStart());

    ba.acceptProduct(std::make_unique<NeckPart>(2));
    EXPECT_FALSE(ba.canStart());

    ba.acceptProduct(std::make_unique<BodyPart>(3));
    EXPECT_TRUE(ba.canStart());
}

TEST(PhaseAssemblers, BodyAssemblerProducesAssembledBodyConsumingOneOfEach) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    BodyAssembler ba("BA1", /*pt=*/1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    ba.setOutputConveyor(&out);

    ba.acceptProduct(std::make_unique<HeadPart>(1));
    ba.acceptProduct(std::make_unique<HeadPart>(11));   // 잔여
    ba.acceptProduct(std::make_unique<NeckPart>(2));
    ba.acceptProduct(std::make_unique<BodyPart>(3));

    for (int t = 1; t <= 2; ++t) ba.update(t);

    EXPECT_EQ(ba.getOutputCount(), 1);
    EXPECT_EQ(ba.getInputBufferSize(), 1);   // HeadPart 1개 잔여
    ASSERT_NE(out.slotAt(0), nullptr);
    EXPECT_EQ(out.slotAt(0)->getType(), ProductType::AssembledBody);
    EXPECT_EQ(out.slotAt(0)->getId(), 1);
}

TEST(PhaseAssemblers, BodyAssemblerCompletedCarriesAssembledBody) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    BodyAssembler ba("BA1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    ba.setOutputConveyor(&out);

    ba.acceptProduct(std::make_unique<HeadPart>(1));
    ba.acceptProduct(std::make_unique<NeckPart>(2));
    ba.acceptProduct(std::make_unique<BodyPart>(3));
    for (int t = 1; t <= 2; ++t) ba.update(t);
    broker.flush();

    int completed = 0;
    for (const auto& ev : rec.received) {
        if (ev.sourceId == "BA1" && ev.type == EventType::Completed) {
            ++completed;
            EXPECT_EQ(ev.productType, ProductType::AssembledBody);
        }
    }
    EXPECT_EQ(completed, 1);
}

// ─────────────────────────────────────────────────────────────
// PartAssembler — AssembledBody + ElecPartSet → FinishedGuitar
// ─────────────────────────────────────────────────────────────

TEST(PhaseAssemblers, PartAssemblerNeedsBothInputs) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    PartAssembler pa("PA1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    pa.setOutputConveyor(&out);

    pa.acceptProduct(std::make_unique<AssembledBody>(1));
    EXPECT_FALSE(pa.canStart());
    pa.acceptProduct(std::make_unique<ElecPartSet>(2));
    EXPECT_TRUE(pa.canStart());
}

TEST(PhaseAssemblers, PartAssemblerProducesFinishedGuitar) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 3, broker);
    PartAssembler pa("PA1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    pa.setOutputConveyor(&out);

    pa.acceptProduct(std::make_unique<AssembledBody>(1));
    pa.acceptProduct(std::make_unique<ElecPartSet>(2));
    for (int t = 1; t <= 2; ++t) pa.update(t);

    EXPECT_EQ(pa.getOutputCount(), 1);
    ASSERT_NE(out.slotAt(0), nullptr);
    EXPECT_EQ(out.slotAt(0)->getType(), ProductType::FinishedGuitar);
    EXPECT_EQ(out.slotAt(0)->getId(), 1);
}

// ─────────────────────────────────────────────────────────────
// Drop 모드 — output 손실 (AssembledBody, FinishedGuitar)
// ─────────────────────────────────────────────────────────────

TEST(PhaseAssemblers, BodyAssemblerDropPublishesAssembledBodyDrop) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 1, broker);
    BodyAssembler ba("BA1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    ba.setOutputConveyor(&out);

    out.push(std::make_unique<RawWood>(999), 0);

    ba.acceptProduct(std::make_unique<HeadPart>(1));
    ba.acceptProduct(std::make_unique<NeckPart>(2));
    ba.acceptProduct(std::make_unique<BodyPart>(3));
    for (int t = 1; t <= 2; ++t) ba.update(t);

    broker.flush();
    int drop = 0;
    for (const auto& ev : rec.received) {
        if (ev.sourceId == "BA1" && ev.type == EventType::Drop) {
            ++drop;
            EXPECT_EQ(ev.productType, ProductType::AssembledBody);  // sourceCount=3
        }
    }
    EXPECT_EQ(drop, 1);
}

TEST(PhaseAssemblers, PartAssemblerDropPublishesFinishedGuitarDrop) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;
    Conveyor     out("Cout", 1, broker);
    PartAssembler pa("PA1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    pa.setOutputConveyor(&out);

    out.push(std::make_unique<RawWood>(999), 0);

    pa.acceptProduct(std::make_unique<AssembledBody>(1));
    pa.acceptProduct(std::make_unique<ElecPartSet>(2));
    for (int t = 1; t <= 2; ++t) pa.update(t);

    broker.flush();
    int drop = 0;
    for (const auto& ev : rec.received) {
        if (ev.sourceId == "PA1" && ev.type == EventType::Drop) {
            ++drop;
            EXPECT_EQ(ev.productType, ProductType::FinishedGuitar);  // sourceCount=5
        }
    }
    EXPECT_EQ(drop, 1);
}
