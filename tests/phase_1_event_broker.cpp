// Phase 1 — EventBroker subscribe/publish/flush + 토픽/와일드카드/디스패치 순서 검증.

#include <string>
#include <vector>

#include "common/Event.h"
#include "common/Types.h"
#include "model/event/EventBroker.h"

#include <gtest/gtest.h>

namespace {

struct RecordingHandler : public IEventHandler {
    std::vector<Event> received;
    std::string        label;

    explicit RecordingHandler(std::string l) : label(std::move(l)) {}

    void handle(const Event& ev) override { received.push_back(ev); }
};

struct OrderHandler : public IEventHandler {
    std::vector<std::string>* order;
    std::string               name;

    OrderHandler(std::vector<std::string>* o, std::string n) : order(o), name(std::move(n)) {}

    void handle(const Event& /*ev*/) override { order->push_back(name); }
};

Event makeEvent(EventType type, const std::string& src, int tick = 0) {
    Event e;
    e.type     = type;
    e.sourceId = src;
    e.tick     = tick;
    return e;
}

}  // namespace

TEST(EventBroker, PublishWithoutFlushDoesNotDispatch) {
    EventBroker      broker;
    RecordingHandler h("type");

    broker.subscribe(EventType::Fault, &h);
    broker.publish(makeEvent(EventType::Fault, "M1"));

    EXPECT_TRUE(h.received.empty());
    EXPECT_EQ(broker.queueSize(), 1u);
}

TEST(EventBroker, FlushDispatchesQueuedEvents) {
    EventBroker      broker;
    RecordingHandler h("type");

    broker.subscribe(EventType::Fault, &h);
    broker.publish(makeEvent(EventType::Fault, "M1", 7));
    broker.flush();

    ASSERT_EQ(h.received.size(), 1u);
    EXPECT_EQ(h.received[0].type, EventType::Fault);
    EXPECT_EQ(h.received[0].sourceId, "M1");
    EXPECT_EQ(h.received[0].tick, 7);
    EXPECT_EQ(broker.queueSize(), 0u);
}

TEST(EventBroker, TypeSubscriberIgnoresOtherTypes) {
    EventBroker      broker;
    RecordingHandler h("fault-only");

    broker.subscribe(EventType::Fault, &h);
    broker.publish(makeEvent(EventType::Completed, "M1"));
    broker.flush();

    EXPECT_TRUE(h.received.empty());
}

TEST(EventBroker, TopicSubscriberFiltersBySource) {
    EventBroker      broker;
    RecordingHandler h("topic");

    broker.subscribe(EventType::Fault, "MachineA", &h);

    broker.publish(makeEvent(EventType::Fault, "MachineB"));
    broker.flush();
    EXPECT_TRUE(h.received.empty());

    broker.publish(makeEvent(EventType::Fault, "MachineA"));
    broker.flush();
    EXPECT_EQ(h.received.size(), 1u);
}

TEST(EventBroker, SubscribeAllReceivesEveryEventType) {
    EventBroker      broker;
    RecordingHandler h("global");

    broker.subscribeAll(&h);
    broker.publish(makeEvent(EventType::Fault, "M1"));
    broker.publish(makeEvent(EventType::Completed, "M2"));
    broker.publish(makeEvent(EventType::Backpressure, "C1"));
    broker.flush();

    EXPECT_EQ(h.received.size(), 3u);
}

TEST(EventBroker, DispatchOrderIsGlobalThenTypeThenTopic) {
    EventBroker              broker;
    std::vector<std::string> order;
    OrderHandler             g(&order, "global");
    OrderHandler             t(&order, "type");
    OrderHandler             p(&order, "topic");

    broker.subscribeAll(&g);
    broker.subscribe(EventType::Fault, &t);
    broker.subscribe(EventType::Fault, "M1", &p);

    broker.publish(makeEvent(EventType::Fault, "M1"));
    broker.flush();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], "global");
    EXPECT_EQ(order[1], "type");
    EXPECT_EQ(order[2], "topic");
}

TEST(EventBroker, ClearQueueDiscardsPendingEvents) {
    EventBroker      broker;
    RecordingHandler h("type");

    broker.subscribe(EventType::Fault, &h);
    broker.publish(makeEvent(EventType::Fault, "M1"));
    broker.clearQueue();
    broker.flush();

    EXPECT_TRUE(h.received.empty());
    EXPECT_EQ(broker.queueSize(), 0u);
}
