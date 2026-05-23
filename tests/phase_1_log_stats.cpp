// Phase 1 — EventLog max 200 enforcement + broker subscribeAll 동작 / clear.
// 그리고 Statistics가 broker 이벤트 구독으로 카운터 갱신 / reset 동작.

#include <string>

#include "common/Event.h"
#include "common/LogEntry.h"
#include "common/Types.h"
#include "model/event/EventBroker.h"
#include "model/event/EventLog.h"
#include "model/stats/Statistics.h"

#include <gtest/gtest.h>

namespace {

Event makeEvent(EventType type, const std::string& src, int tick = 0) {
    Event e;
    e.type     = type;
    e.sourceId = src;
    e.tick     = tick;
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

TEST(Statistics, CountersUpdateFromBrokerEvents) {
    EventBroker broker;
    Statistics  s(broker);

    // Started ×3 → wip +3
    broker.publish(makeEvent(EventType::Started, "Spawner", 1));
    broker.publish(makeEvent(EventType::Started, "Spawner", 2));
    broker.publish(makeEvent(EventType::Started, "Spawner", 3));
    // Completed → finished +1, wip -1
    broker.publish(makeEvent(EventType::Completed, "Packager", 4));
    // Drop → lost +1, wip -1
    broker.publish(makeEvent(EventType::Drop, "C1", 5));
    // Fault → breakdowns +1
    broker.publish(makeEvent(EventType::Fault, "M1", 6));
    broker.flush();

    EXPECT_EQ(s.getFinished(), 1);
    EXPECT_EQ(s.getWip(), 1);          // +3 -1 -1
    EXPECT_EQ(s.getBreakdowns(), 1);
    EXPECT_EQ(s.getLost(), 1);
}

TEST(Statistics, IgnoresUnsubscribedEventTypes) {
    EventBroker broker;
    Statistics  s(broker);

    // Resume / Backpressure는 Statistics 관심 밖
    broker.publish(makeEvent(EventType::Resume, "M1", 1));
    broker.publish(makeEvent(EventType::Backpressure, "C1", 2));
    broker.flush();

    EXPECT_EQ(s.getFinished(), 0);
    EXPECT_EQ(s.getWip(), 0);
    EXPECT_EQ(s.getBreakdowns(), 0);
    EXPECT_EQ(s.getLost(), 0);
}

TEST(Statistics, ResetZerosAllCounters) {
    EventBroker broker;
    Statistics  s(broker);

    broker.publish(makeEvent(EventType::Started, "Spawner", 1));
    broker.publish(makeEvent(EventType::Drop, "C1", 2));
    broker.publish(makeEvent(EventType::Fault, "M1", 3));
    broker.flush();

    s.reset();

    EXPECT_EQ(s.getFinished(), 0);
    EXPECT_EQ(s.getWip(), 0);
    EXPECT_EQ(s.getBreakdowns(), 0);
    EXPECT_EQ(s.getLost(), 0);
}
