// Phase 1 — EventLog max 200 enforcement + broker subscribeAll 동작 / clear.
// Statistics가 broker 이벤트 구독으로 카운터 갱신 / reset 동작.
// WIP 회계는 productType 기반 sourceCount (Spawned/Packaged/Drop).

#include <optional>
#include <string>

#include "common/Event.h"
#include "common/LogEntry.h"
#include "common/Types.h"
#include "model/event/EventBroker.h"
#include "model/event/EventLog.h"
#include "model/stats/Statistics.h"

#include <gtest/gtest.h>

namespace {

Event makeEvent(EventType type, const std::string& src, int tick = 0,
                std::optional<int> pid = std::nullopt,
                std::optional<ProductType> ptype = std::nullopt) {
    Event e;
    e.type        = type;
    e.sourceId    = src;
    e.tick        = tick;
    e.productId   = pid;
    e.productType = ptype;
    return e;
}

}  // namespace

TEST(EventLog, SubscribesAllAndCapturesEverythingPublished) {
    EventBroker broker;
    EventLog    log(broker);

    broker.publish(makeEvent(EventType::Fault, "M1", 5));
    broker.publish(makeEvent(EventType::Completed, "M2", 6));
    broker.flush();

    auto logs = log.getLogs();
    ASSERT_EQ(logs.size(), 2u);
    EXPECT_EQ(logs[0].tick, 5);
    EXPECT_EQ(logs[0].sourceId, "M1");
    EXPECT_NE(logs[0].message.find("Fault"), std::string::npos);
    EXPECT_NE(logs[0].message.find("M1"), std::string::npos);
    EXPECT_EQ(logs[1].sourceId, "M2");
    EXPECT_NE(logs[1].message.find("Completed"), std::string::npos);
}

TEST(EventLog, EnforcesMaxEntriesWithFifoDrop) {
    EventBroker broker;
    EventLog    log(broker);

    for (int i = 0; i < 201; ++i) {
        broker.publish(makeEvent(EventType::Started, "M" + std::to_string(i), i));
    }
    broker.flush();

    EXPECT_EQ(log.size(), 200u);
    auto logs = log.getLogs();
    EXPECT_EQ(logs.front().tick, 1);    // 가장 오래된 tick=0이 drop됨
    EXPECT_EQ(logs.back().tick, 200);
}

TEST(EventLog, ClearEmptiesEntries) {
    EventBroker broker;
    EventLog    log(broker);

    broker.publish(makeEvent(EventType::Fault, "M1"));
    broker.flush();
    EXPECT_EQ(log.size(), 1u);

    log.clear();
    EXPECT_EQ(log.size(), 0u);
}

TEST(Statistics, AllCountersStartAtZero) {
    EventBroker broker;
    Statistics  s(broker);

    EXPECT_EQ(s.getFinished(), 0);
    EXPECT_EQ(s.getWip(), 0);
    EXPECT_EQ(s.getBreakdowns(), 0);
    EXPECT_EQ(s.getLost(), 0);
}

TEST(Statistics, SpawnedIncrementsWipBySourceCount) {
    EventBroker broker;
    Statistics  s(broker);

    // RawWood / Bridge / Pickup 각각 sourceCount = 1
    broker.publish(makeEvent(EventType::Spawned, "WoodSpawner", 1, 1, ProductType::RawWood));
    broker.publish(makeEvent(EventType::Spawned, "WoodSpawner", 2, 2, ProductType::RawWood));
    broker.publish(makeEvent(EventType::Spawned, "BridgeSpawner", 3, 3, ProductType::Bridge));
    broker.flush();

    EXPECT_EQ(s.getWip(), 3);
    EXPECT_EQ(s.getFinished(), 0);
    EXPECT_EQ(s.getLost(), 0);
}

TEST(Statistics, PackagedDecrementsWipBySourceCountAndIncrementsFinished) {
    EventBroker broker;
    Statistics  s(broker);

    // 5번 Spawn (총 wip=5)
    for (int i = 0; i < 5; ++i) {
        broker.publish(makeEvent(EventType::Spawned, "Spawner", i, i, ProductType::RawWood));
    }
    // Packager가 FinishedGuitar 1개 출하 (sourceCount=5 → wip -= 5)
    broker.publish(makeEvent(EventType::Packaged, "Packager", 10, 100, ProductType::FinishedGuitar));
    broker.flush();

    EXPECT_EQ(s.getWip(), 0);
    EXPECT_EQ(s.getFinished(), 1);
    EXPECT_EQ(s.getLost(), 0);
}

TEST(Statistics, DropOfAssembledBodyDecrementsBySourceCount3) {
    EventBroker broker;
    Statistics  s(broker);

    // 3번 Spawn (wip=3)
    broker.publish(makeEvent(EventType::Spawned, "WoodSpawner", 1, 1, ProductType::RawWood));
    broker.publish(makeEvent(EventType::Spawned, "WoodSpawner", 2, 2, ProductType::RawWood));
    broker.publish(makeEvent(EventType::Spawned, "WoodSpawner", 3, 3, ProductType::RawWood));
    // BodyAssembler가 AssembledBody 생성했다가 conveyor 가득차서 Drop (sourceCount=3 → wip -= 3, lost += 3)
    broker.publish(makeEvent(EventType::Drop, "BodyAssembler", 5, 42, ProductType::AssembledBody));
    broker.flush();

    EXPECT_EQ(s.getWip(), 0);
    EXPECT_EQ(s.getLost(), 3);
}

TEST(Statistics, FaultIncrementsBreakdowns) {
    EventBroker broker;
    Statistics  s(broker);

    broker.publish(makeEvent(EventType::Fault, "HeadCutter", 1));
    broker.publish(makeEvent(EventType::Fault, "Packager", 2));
    broker.flush();

    EXPECT_EQ(s.getBreakdowns(), 2);
}

TEST(Statistics, IgnoresLifecycleAndResumeBackpressureEvents) {
    EventBroker broker;
    Statistics  s(broker);

    // Started / Completed / Resume / Backpressure 는 Statistics 관심 밖
    broker.publish(makeEvent(EventType::Started, "HeadCutter", 1, 1, ProductType::RawWood));
    broker.publish(makeEvent(EventType::Completed, "HeadCutter", 2, 1, ProductType::HeadPart));
    broker.publish(makeEvent(EventType::Resume, "M1", 3));
    broker.publish(makeEvent(EventType::Backpressure, "C1", 4));
    broker.flush();

    EXPECT_EQ(s.getFinished(), 0);
    EXPECT_EQ(s.getWip(), 0);
    EXPECT_EQ(s.getBreakdowns(), 0);
    EXPECT_EQ(s.getLost(), 0);
}

TEST(Statistics, ResetZerosAllCounters) {
    EventBroker broker;
    Statistics  s(broker);

    broker.publish(makeEvent(EventType::Spawned, "Spawner", 1, 1, ProductType::RawWood));
    broker.publish(makeEvent(EventType::Drop, "M1", 2, 2, ProductType::RawWood));
    broker.publish(makeEvent(EventType::Fault, "M1", 3));
    broker.flush();

    s.reset();

    EXPECT_EQ(s.getFinished(), 0);
    EXPECT_EQ(s.getWip(), 0);
    EXPECT_EQ(s.getBreakdowns(), 0);
    EXPECT_EQ(s.getLost(), 0);
}
