// Phase 3 Slice 3 — Painter.
// BodyPart 입력을 받아 isPainted=true인 새 BodyPart로 변환 (새 id 발급).

#include <memory>
#include <random>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/conveyor/Conveyor.h"
#include "model/event/EventBroker.h"
#include "model/machine/MachineStates.h"
#include "model/machine/painter/Painter.h"
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

TEST(PhasePainter, EmitsPaintedBodyPartWithNewId) {
    EventBroker  broker;
    auto         rng = makeRng();
    ProductIdGen idGen;

    Conveyor out("Cout", 3, broker);
    Painter  p("P1", /*pt=*/1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    p.setOutputConveyor(&out);

    p.acceptProduct(std::make_unique<BodyPart>(42));   // unpainted input
    for (int t = 1; t <= 2; ++t) p.update(t);

    ASSERT_NE(out.slotAt(0), nullptr);
    EXPECT_EQ(out.slotAt(0)->getType(), ProductType::BodyPart);
    EXPECT_NE(out.slotAt(0)->getId(), 42);   // input id 폐기, 새 id 발급
    EXPECT_EQ(out.slotAt(0)->getId(), 1);

    const auto* painted = static_cast<const BodyPart*>(out.slotAt(0));
    EXPECT_TRUE(painted->isPainted());
}

TEST(PhasePainter, PublishesStartedAndCompletedAroundProcess) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;

    Conveyor out("Cout", 3, broker);
    Painter  p("P1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    p.setOutputConveyor(&out);

    p.acceptProduct(std::make_unique<BodyPart>(42));
    for (int t = 1; t <= 2; ++t) p.update(t);
    broker.flush();

    int started = 0, completed = 0;
    for (const auto& ev : rec.received) {
        if (ev.sourceId != "P1") continue;
        if (ev.type == EventType::Started) {
            ++started;
            EXPECT_EQ(ev.productType, ProductType::BodyPart);
            EXPECT_EQ(*ev.productId, 42);     // input id
        }
        if (ev.type == EventType::Completed) {
            ++completed;
            EXPECT_EQ(ev.productType, ProductType::BodyPart);
            EXPECT_EQ(*ev.productId, 1);      // new id
        }
    }
    EXPECT_EQ(started, 1);
    EXPECT_EQ(completed, 1);
}

TEST(PhasePainter, DropModePublishesDropOnFullConveyor) {
    EventBroker   broker;
    EventRecorder rec;
    broker.subscribeAll(&rec);
    auto         rng = makeRng();
    ProductIdGen idGen;

    Conveyor out("Cout", 1, broker);
    Painter  p("P1", 1, 0.0, OverflowMode::Drop, broker, rng, idGen);
    p.setOutputConveyor(&out);

    out.push(std::make_unique<RawWood>(999), 0);
    ASSERT_FALSE(out.canAccept());

    p.acceptProduct(std::make_unique<BodyPart>(42));
    for (int t = 1; t <= 2; ++t) p.update(t);

    EXPECT_EQ(p.getOutputCount(), 0);

    broker.flush();
    int drop = 0, completed = 0;
    for (const auto& ev : rec.received) {
        if (ev.sourceId == "P1") {
            if (ev.type == EventType::Drop) {
                ++drop;
                EXPECT_EQ(ev.productType, ProductType::BodyPart);
            }
            if (ev.type == EventType::Completed) ++completed;
        }
    }
    EXPECT_EQ(drop, 1);
    EXPECT_EQ(completed, 0);
}
