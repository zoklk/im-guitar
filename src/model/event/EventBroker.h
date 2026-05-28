#pragma once

#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "Event.h"
#include "Types.h"

class EventBroker {
public:
    void subscribe(EventType type, IEventHandler* handler);
    void subscribe(EventType type, const std::string& sourceId, IEventHandler* handler);
    void subscribeAll(IEventHandler* handler);

    void publish(const Event& event);
    void flush();

    void clearQueue();
    void clearTopicSubscriptions();         // Machine 토픽 구독 일괄 제거 (Factory.reset 용)
    void restoreQueue(const std::vector<Event>& events);  // pendingEvents 복원 (메멘토)

    std::vector<Event> snapshotQueue() const;
    size_t             queueSize() const { return eventQueue_.size(); }

private:
    std::unordered_map<EventType, std::vector<IEventHandler*>>                                       typeSubs_;
    std::unordered_map<EventType, std::unordered_map<std::string, std::vector<IEventHandler*>>>     topicSubs_;
    std::vector<IEventHandler*>                                                                     globalSubs_;
    std::queue<Event>                                                                               eventQueue_;
};
