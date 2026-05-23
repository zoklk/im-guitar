#include "EventLog.h"

#include <string>

namespace {

const char* eventTypeName(EventType type) {
    switch (type) {
        case EventType::Fault:        return "Fault";
        case EventType::Resume:       return "Resume";
        case EventType::Started:      return "Started";
        case EventType::Completed:    return "Completed";
        case EventType::Backpressure: return "Backpressure";
    }
    return "Unknown";
}

std::string formatMessage(const Event& ev) {
    std::string msg;
    msg.reserve(32);
    msg.push_back('[');
    msg.append(eventTypeName(ev.type));
    msg.append("] ");
    msg.append(ev.sourceId);
    return msg;
}

}  // namespace

EventLog::EventLog(EventBroker& broker) {
    broker.subscribeAll(this);
}

void EventLog::handle(const Event& ev) {
    LogEntry entry;
    entry.tick     = ev.tick;
    entry.sourceId = ev.sourceId;
    entry.message  = formatMessage(ev);
    pushEntry(std::move(entry));
}

void EventLog::appendDirect(const LogEntry& entry) {
    pushEntry(entry);
}

void EventLog::clear() {
    entries_.clear();
}

std::vector<LogEntry> EventLog::getLogs() const {
    return std::vector<LogEntry>(entries_.begin(), entries_.end());
}

void EventLog::pushEntry(LogEntry entry) {
    if (entries_.size() >= kMaxEntries) {
        entries_.pop_front();
    }
    entries_.push_back(std::move(entry));
}
