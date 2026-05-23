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

    size_t queueSize() const { return eventQueue_.size(); }

private:
    std::unordered_map<EventType, std::vector<IEventHandler*>>                                       typeSubs_;
    std::unordered_map<EventType, std::unordered_map<std::string, std::vector<IEventHandler*>>>     topicSubs_;
    std::vector<IEventHandler*>                                                                     globalSubs_;
    std::queue<Event>                                                                               eventQueue_;
};
