// Phase 1 — EventLog max 200 enforcement + broker subscribeAll 동작 + appendDirect/clear.
// 그리고 Statistics 4 카운터 증감 / reset 동작.

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

TEST(EventLog, AppendDirectBypassesBroker) {
    EventBroker broker;
    EventLog    log(broker);

    LogEntry entry;
    entry.tick     = 42;
    entry.sourceId = "ConveyorX";
    entry.message  = "overflow drop";
    log.appendDirect(entry);

    auto logs = log.getLogs();
    ASSERT_EQ(logs.size(), 1u);
    EXPECT_EQ(logs[0].tick, 42);
    EXPECT_EQ(logs[0].sourceId, "ConveyorX");
    EXPECT_EQ(logs[0].message, "overflow drop");
}

TEST(Statistics, AllCountersStartAtZero) {
    Statistics s;
    EXPECT_EQ(s.getFinished(), 0);
    EXPECT_EQ(s.getWip(), 0);
    EXPECT_EQ(s.getBreakdowns(), 0);
    EXPECT_EQ(s.getLost(), 0);
}

TEST(Statistics, IncrementersAndDecrementerWork) {
    Statistics s;
    s.incFinished();
    s.incFinished();
    s.incWip();
    s.incWip();
    s.incWip();
    s.decWip();
    s.incBreakdowns();
    s.incLost();

    EXPECT_EQ(s.getFinished(), 2);
    EXPECT_EQ(s.getWip(), 2);
    EXPECT_EQ(s.getBreakdowns(), 1);
    EXPECT_EQ(s.getLost(), 1);
}

TEST(Statistics, ResetZerosAllCounters) {
    Statistics s;
    s.incFinished();
    s.incWip();
    s.incBreakdowns();
    s.incLost();
    s.reset();

    EXPECT_EQ(s.getFinished(), 0);
    EXPECT_EQ(s.getWip(), 0);
    EXPECT_EQ(s.getBreakdowns(), 0);
    EXPECT_EQ(s.getLost(), 0);
}
