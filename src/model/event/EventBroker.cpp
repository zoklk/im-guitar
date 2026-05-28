#include "EventBroker.h"

void EventBroker::subscribe(EventType type, IEventHandler* handler) {
    if (handler == nullptr) return;
    typeSubs_[type].push_back(handler);
}

void EventBroker::subscribe(EventType type, const std::string& sourceId, IEventHandler* handler) {
    if (handler == nullptr) return;
    topicSubs_[type][sourceId].push_back(handler);
}

void EventBroker::subscribeAll(IEventHandler* handler) {
    if (handler == nullptr) return;
    globalSubs_.push_back(handler);
}

void EventBroker::publish(const Event& event) {
    eventQueue_.push(event);
}

void EventBroker::flush() {
    while (!eventQueue_.empty()) {
        const Event ev = eventQueue_.front();
        eventQueue_.pop();

        for (IEventHandler* h : globalSubs_) {
            h->handle(ev);
        }

        auto typeIt = typeSubs_.find(ev.type);
        if (typeIt != typeSubs_.end()) {
            for (IEventHandler* h : typeIt->second) {
                h->handle(ev);
            }
        }

        auto topicTypeIt = topicSubs_.find(ev.type);
        if (topicTypeIt != topicSubs_.end()) {
            auto sourceIt = topicTypeIt->second.find(ev.sourceId);
            if (sourceIt != topicTypeIt->second.end()) {
                for (IEventHandler* h : sourceIt->second) {
                    h->handle(ev);
                }
            }
        }
    }
}

void EventBroker::clearQueue() {
    std::queue<Event> empty;
    std::swap(eventQueue_, empty);
}

void EventBroker::clearTopicSubscriptions() {
    topicSubs_.clear();
}

void EventBroker::restoreQueue(const std::vector<Event>& events) {
    std::queue<Event> empty;
    std::swap(eventQueue_, empty);
    for (const auto& ev : events) {
        eventQueue_.push(ev);
    }
}

std::vector<Event> EventBroker::snapshotQueue() const {
    std::vector<Event> out;
    out.reserve(eventQueue_.size());
    std::queue<Event> copy = eventQueue_;
    while (!copy.empty()) {
        out.push_back(copy.front());
        copy.pop();
    }
    return out;
}
