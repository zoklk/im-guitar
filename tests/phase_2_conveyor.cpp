// Phase 2 — Conveyor 슬롯 시프트 / 출구 방출 / setDownstream 와이어링 검증.
// 가득찬 conveyor에 push가 도달하면 std::abort (Machine 책임 위반의 dead-code 가드).
// Overflow 정책(Drop/Backpressure)은 이제 Machine 책임 → 관련 테스트는 phase_3에서.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/conveyor/Conveyor.h"
#include "model/event/EventBroker.h"
#include "model/machine/IMachine.h"
#include "model/product/Product.h"

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

std::unique_ptr<RawWood> makeWood(int id) {
    return std::make_unique<RawWood>(id);
}

}  // namespace

TEST(Conveyor, PushFillsEntrySlot) {
    EventBroker broker;

    Conveyor c("C1", 5, broker);

    EXPECT_TRUE(c.canAccept());
    c.push(makeWood(101), /*tick=*/0);

    ASSERT_NE(c.slotAt(0), nullptr);
    EXPECT_EQ(c.slotAt(0)->getId(), 101);
    EXPECT_FALSE(c.canAccept());
}

TEST(Conveyor, ShiftAdvancesProductTowardsExit) {
    EventBroker broker;

    Conveyor c("C1", 5, broker);
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
    MockMachine mock;

    Conveyor c("C1", 3, broker);
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

TEST(Conveyor, CanAcceptTracksEntrySlot) {
    EventBroker broker;

    Conveyor c("C1", 3, broker);

    EXPECT_TRUE(c.canAccept());
    c.push(makeWood(1), 0);
    EXPECT_FALSE(c.canAccept());

    c.update(1);  // shift: 입구 비고 중간 채워짐
    EXPECT_TRUE(c.canAccept());
}

TEST(Conveyor, ReverseShiftCondensesGapTowardsEntry) {
    EventBroker broker;

    // 초기 [1,1,1,1,0], downstream=null → 출구 비어있으니 한 틱 후 [0,1,1,1,1]
    Conveyor c("C1", 5, broker);

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
    // downstream을 wire하지 않음 → 출구 슬롯이 영구 점유

    Conveyor c("C1", 3, broker);

    c.push(makeWood(1), 0);
    c.update(1);  // [_, 1, _]
    c.update(2);  // [_, _, 1]
    c.update(3);  // 출구 막힘 (downstream null), [_, _, 1] 유지

    c.push(makeWood(2), 0);
    c.update(4);  // [_, 2, 1]
    c.push(makeWood(3), 0);
    c.update(5);  // [3, 2, 1]

    EXPECT_NE(c.slotAt(0), nullptr);
    EXPECT_NE(c.slotAt(1), nullptr);
    EXPECT_NE(c.slotAt(2), nullptr);
    EXPECT_EQ(c.slotAt(2)->getId(), 1);  // 출구는 가장 먼저 들어간 1
    EXPECT_FALSE(c.canAccept());
}

TEST(Conveyor, WiringIsDeferredViaSetDownstream) {
    EventBroker broker;
    MockMachine mock;

    Conveyor c("C1", 2, broker);

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
