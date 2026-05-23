// Phase 2 — Conveyor 슬롯 시프트 / 출구 방출 / overflow Drop / overflow Backpressure /
//           setDownstream 와이어링 검증. MockMachine으로 IMachine 주입.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/conveyor/Conveyor.h"
#include "model/event/EventBroker.h"
#include "model/event/EventLog.h"
#include "model/machine/IMachine.h"
#include "model/product/Product.h"
#include "model/stats/Statistics.h"

#include <gtest/gtest.h>

namespace {

struct MockMachine : public IMachine {
    std::vector<std::unique_ptr<Product>> received;
    std::string                           id_ = "M_mock";

    void acceptProduct(std::unique_ptr<Product> p) override {
        received.push_back(std::move(p));
    }
    const std::string& getId() const override { return id_; }
};

struct EventRecorder : public IEventHandler {
    std::vector<Event> received;
    void handle(const Event& ev) override { received.push_back(ev); }
};

std::unique_ptr<RawWood> makeWood(int id) {
    return std::make_unique<RawWood>(id);
}

}  // namespace

TEST(Conveyor, PushFillsEntrySlot) {
    EventBroker broker;
    EventLog    log(broker);
    Statistics  stats;

    Conveyor c("C1", 5, OverflowMode::Drop, broker, log, stats);

    EXPECT_TRUE(c.canAccept());
    c.push(makeWood(101), /*tick=*/0);

    ASSERT_NE(c.slotAt(0), nullptr);
    EXPECT_EQ(c.slotAt(0)->getId(), 101);
    EXPECT_FALSE(c.canAccept());
}

TEST(Conveyor, ShiftAdvancesProductTowardsExit) {
    EventBroker broker;
    EventLog    log(broker);
    Statistics  stats;

    Conveyor c("C1", 5, OverflowMode::Drop, broker, log, stats);
    c.push(makeWood(42), /*tick=*/0);

    // length=5 → 입구(0)에서 출구(4)까지 4틱
    for (int t = 1; t <= 4; ++t) {
        c.update(t);
    }

    EXPECT_EQ(c.slotAt(0), nullptr);
    ASSERT_NE(c.slotAt(4), nullptr);
    EXPECT_EQ(c.slotAt(4)->getId(), 42);
}

TEST(Conveyor, ExitSlotDispatchesToDownstream) {
    EventBroker broker;
    EventLog    log(broker);
    Statistics  stats;
    MockMachine mock;

    Conveyor c("C1", 3, OverflowMode::Drop, broker, log, stats);
    c.setDownstream(&mock);

    c.push(makeWood(7), /*tick=*/0);
    // 3슬롯이라 2틱이면 출구 도달, 3틱째 update에서 방출
    c.update(1);  // [_, P, _]
    c.update(2);  // [_, _, P]
    c.update(3);  // 출구 슬롯 P → mock.acceptProduct, shift는 no-op

    EXPECT_EQ(c.slotAt(2), nullptr);
    ASSERT_EQ(mock.received.size(), 1u);
    EXPECT_EQ(mock.received[0]->getId(), 7);
}

TEST(Conveyor, OverflowDropIncrementsLostAndLogsTick) {
    EventBroker broker;
    EventLog    log(broker);
    Statistics  stats;

    Conveyor c("C1", 1, OverflowMode::Drop, broker, log, stats);

    c.push(makeWood(1), /*tick=*/0);   // 슬롯 점유
    EXPECT_FALSE(c.canAccept());

    stats.incWip();  // drop이 wip 감소시키므로 사전에 1 적재
    c.push(makeWood(2), /*tick=*/42);  // overflow → drop

    EXPECT_EQ(stats.getLost(), 1);
    EXPECT_EQ(stats.getWip(), 0);

    auto logs = log.getLogs();
    ASSERT_FALSE(logs.empty());
    EXPECT_EQ(logs.back().tick, 42);
    EXPECT_EQ(logs.back().sourceId, "C1");
    EXPECT_EQ(logs.back().message, "overflow drop");
}

TEST(Conveyor, OverflowBackpressurePublishesAndDoesNotCountLost) {
    EventBroker   broker;
    EventLog      log(broker);
    Statistics    stats;
    EventRecorder rec;
    broker.subscribe(EventType::Backpressure, "C1", &rec);

    Conveyor c("C1", 1, OverflowMode::Backpressure, broker, log, stats);

    c.push(makeWood(1), /*tick=*/0);
    c.push(makeWood(2), /*tick=*/11);  // overflow → publish Backpressure
    broker.flush();

    EXPECT_EQ(stats.getLost(), 0);
    ASSERT_EQ(rec.received.size(), 1u);
    EXPECT_EQ(rec.received[0].type, EventType::Backpressure);
    EXPECT_EQ(rec.received[0].sourceId, "C1");
    EXPECT_EQ(rec.received[0].tick, 11);
}

TEST(Conveyor, CanAcceptTracksEntrySlot) {
    EventBroker broker;
    EventLog    log(broker);
    Statistics  stats;

    Conveyor c("C1", 3, OverflowMode::Drop, broker, log, stats);

    EXPECT_TRUE(c.canAccept());
    c.push(makeWood(1), 0);
    EXPECT_FALSE(c.canAccept());

    c.update(1);  // shift: 입구 비고 중간 채워짐
    EXPECT_TRUE(c.canAccept());
}

TEST(Conveyor, ReverseShiftCondensesGapTowardsEntry) {
    EventBroker broker;
    EventLog    log(broker);
    Statistics  stats;

    // 초기 [1,1,1,1,0], downstream=null → 출구 비어있으니 한 틱 후 [0,1,1,1,1]
    Conveyor c("C1", 5, OverflowMode::Drop, broker, log, stats);

    c.push(makeWood(11), 0);
    c.update(1);  // [_, 11, _, _, _]
    c.push(makeWood(12), 0);
    c.update(2);  // [_, 12, 11, _, _]
    c.push(makeWood(13), 0);
    c.update(3);  // [_, 13, 12, 11, _]
    c.push(makeWood(14), 0);
    // 이 시점 [14, 13, 12, 11, _] — push가 slot0 채움

    EXPECT_NE(c.slotAt(0), nullptr);
    EXPECT_NE(c.slotAt(1), nullptr);
    EXPECT_NE(c.slotAt(2), nullptr);
    EXPECT_NE(c.slotAt(3), nullptr);
    EXPECT_EQ(c.slotAt(4), nullptr);

    c.update(4);  // 역방향 시프트로 가운데 구멍 없이 출구쪽 응축

    EXPECT_EQ(c.slotAt(0), nullptr);
    ASSERT_NE(c.slotAt(1), nullptr);
    ASSERT_NE(c.slotAt(2), nullptr);
    ASSERT_NE(c.slotAt(3), nullptr);
    ASSERT_NE(c.slotAt(4), nullptr);
    EXPECT_EQ(c.slotAt(1)->getId(), 14);
    EXPECT_EQ(c.slotAt(2)->getId(), 13);
    EXPECT_EQ(c.slotAt(3)->getId(), 12);
    EXPECT_EQ(c.slotAt(4)->getId(), 11);
}

TEST(Conveyor, BlockedDownstreamCausesPileUp) {
    EventBroker broker;
    EventLog    log(broker);
    Statistics  stats;
    // downstream을 wire하지 않음 → 출구 슬롯이 영구 점유

    Conveyor c("C1", 3, OverflowMode::Drop, broker, log, stats);

    c.push(makeWood(1), 0);
    c.update(1);  // [_, 1, _]
    c.update(2);  // [_, _, 1]
    c.update(3);  // 출구 막힘 (downstream null), [_, _, 1] 유지

    c.push(makeWood(2), 0);
    c.update(4);  // [_, 2, 1]
    c.push(makeWood(3), 0);
    c.update(5);  // [_, 3, 1] (slot2는 점유, slot1 끌어당김 없음 → wait, 검증 필요)

    // update(5) 직전 [3, 2, 1]. update: 출구막힘, 시프트 i=2(점유,no), i=1(점유,no) → [3, 2, 1] 유지
    EXPECT_NE(c.slotAt(0), nullptr);
    EXPECT_NE(c.slotAt(1), nullptr);
    EXPECT_NE(c.slotAt(2), nullptr);
    EXPECT_EQ(c.slotAt(2)->getId(), 1);  // 출구는 가장 먼저 들어간 1
    EXPECT_FALSE(c.canAccept());
}

TEST(Conveyor, WiringIsDeferredViaSetDownstream) {
    EventBroker broker;
    EventLog    log(broker);
    Statistics  stats;
    MockMachine mock;

    Conveyor c("C1", 2, OverflowMode::Drop, broker, log, stats);

    c.push(makeWood(99), 0);
    c.update(1);  // [_, 99]
    c.update(2);  // downstream=null → 99 유지

    ASSERT_NE(c.slotAt(1), nullptr);
    EXPECT_TRUE(mock.received.empty());

    c.setDownstream(&mock);
    c.update(3);  // 이번엔 방출

    EXPECT_EQ(c.slotAt(1), nullptr);
    ASSERT_EQ(mock.received.size(), 1u);
    EXPECT_EQ(mock.received[0]->getId(), 99);
}
